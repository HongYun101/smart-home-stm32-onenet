/* ============================================================
 * app_main.c —— 应用程序主文件
 *
 * 项目：STM32F103C8T6 + ESP8266 + OneNET MQTT 物联网设备
 * 功能：
 *   ① 系统/板级/应用三层初始化
 *   ② ESP8266 WiFi 连接 + MQTT 连接 + 主题订阅
 *   ③ OneNET 云平台属性上报（上行）+ 属性设置（下发）
 *   ④ 本地 LED1 / 继电器(JDQ) 控制
 *
 * 核心数据流：
 *   传感器 → onenet_data[] → onenet_mqtt_publish_post_data() → ESP8266 → OneNET
 *   OneNET → ESP8266 → 串口中断 → AT 驱动解析 → 主题匹配 → 用户回调 → 执行动作
 * ============================================================ */
 
#include "stm32f1xx_hal.h"
// 应用程序主函数声明（app_main 在这里实现）
#include "app_main.h"
// OneNET MQTT 协议层：JSON 封装/解析、主题订阅、属性上下行
#include "app_onenet_mqtt.h"

// AT 指令驱动层：at_init()、at_send_cmd()、环形缓冲区管理
#include "at_driver.h"

// ESP8266 驱动层：WiFi 连接、MQTT 连接、订阅/发布 AT 指令封装
#include "esp8266.h"

// 串口初始化（huart1 调试、huart3 ESP8266）
#include "usart.h"
// 定时器初始化（htim3 用于 PWM 控制 LED 亮度）
#include "tim.h"

// 字符串操作（strcmp 用于下发数据时匹配属性名）
#include "string.h"

//C库
#include <stdio.h>

//dht11传感器 温湿度
#include "dht11_hal.h"	

//oled 硬件驱动 屏幕
#include "OLED.h"	
#include "OLED_Data.h"   //oled字库

//bh1750传感器 光照
#include "bh1750.h"

//mq-2气体检测
#include "mq2.h"

/* ============================================================
 * WiFi 配置信息结构体
 *
 * .ssid     = 你要连接的 WiFi 热点名称
 * .password = WiFi 密码
 *
 * ⚠️ 注意：ESP8266 只支持 2.4GHz WiFi，5G 频段连不上
 * ============================================================ */
wifi_info_t wifi_info = {
	.ssid     = "YOUR_WIFI_SSID",        // ← 改成你自己的 WiFi 名
	.password = "YOUR_WIFI_PASSWORD",     // ← 改成你自己的 WiFi 密码
};

/* ============================================================
 * MQTT 配置信息结构体
 *
 * OneNET MQTT 接入地址：mqtts.heclouds.com
 * 端口 1883 = 非加密 MQTT（1883 也可以用于 OneNET）
 *
 * 产品 ID：YOUR_PRODUCT_ID
 * 设备 ID：dev-001
 *
 * OneNET 新版 MQTT 物模型 Topic 格式：
 *   $sys/{产品ID}/{设备ID}/thing/property/post       → 属性上报（发布）
 *   $sys/{产品ID}/{设备ID}/thing/property/post/reply → 属性上报响应（订阅）
 *   $sys/{产品ID}/{设备ID}/thing/property/set        → 属性设置（订阅）
 *   $sys/{产品ID}/{设备ID}/thing/property/set_reply  → 属性设置响应（发布）
 *
 * 即：
 *   - 设备主动上报数据：设备 publish → 平台 reply
 *   - 平台下发指令：平台 publish → 设备 reply
 * ============================================================ */
mqtt_info_t mqtt_info = {
	.host       = "mqtts.heclouds.com",   // OneNET MQTT 服务器域名
	.port       = 1883,                    // MQTT 标准端口
	.client_id  = "dev-001",              // 设备编号（OneNET 平台注册的）
	.username   = "YOUR_PRODUCT_ID",           // 产品 ID（OneNET 平台分配）

	/* password 由 OneNET 平台自动生成，包含鉴权令牌和过期时间
	 * 格式：version=2018-10-31&res=products%2F{产品ID}%2Fdevices%2F{设备ID}&et={过期时间戳}&method=md5&sign={签名}
	 *
	 * %2F 是 URL 编码的 "/"，实际路径是 products/YOUR_PRODUCT_ID/devices/dev-001
	 * et=1907299006 → token 过期时间（Unix 时间戳）
	 *                 1907299006 = 2030-06-11 左右，token 还有效
	 * sign=...        → MD5 签名（产品 access_key + token 做 MD5）
	 */
	.password = "version=2018-10-31&res=products%2FYOUR_PRODUCT_ID%2Fdevices%2Fdev-001&et=1907299006&method=md5&sign=YOUR_SIGN",

	/* ---------- 四个 OneNET Topic ---------- */

	// 属性上报 → 设备主动发数据给云平台
	.publish_topic_post   = "$sys/YOUR_PRODUCT_ID/dev-001/thing/property/post",

	// 属性上报响应 → 平台回复"收到了"，设备订阅这个来确认上报是否成功
	.subscribe_topic_post = "$sys/YOUR_PRODUCT_ID/dev-001/thing/property/post/reply",

	// 属性设置响应 → 设备回复平台"我执行了"
	.publish_topic_set    = "$sys/YOUR_PRODUCT_ID/dev-001/thing/property/set_reply",

	// 属性设置 → 平台下发指令给设备（如手机 APP 点"开灯"就是走这个 topic）
	.subscribe_topic_set  = "$sys/YOUR_PRODUCT_ID/dev-001/thing/property/set",
};

/* ============================================================
 * 全局数据结构
 *
 * onenet_topic_data   — 存储从 MQTT 消息中解析出的主题数据
 * onenet_propertySet  — 存储平台下发属性设置指令的完整内容
 * ============================================================ */
onenet_mqtt_topic_data_t onenet_topic_data   = {0};  // MQTT 主题数据（清零初始化）
onenet_mqtt_propertySet_t onenet_propertySet = {0};  // MQTT 属性设置数据（清零初始化）

/* ============================================================
 * fputc() —— printf 输出重定向
 *
 * 标准 C 库的 printf() 最终调用这个函数把字符发出去
 * 重写它，把 printf 的输出转发到 USART1（调试串口）
 *
 * 效果：调用 printf("Hello") → 从串口助手就能看到 "Hello"
 *
 * 参数：
 *   ch   = 要发送的一个字符
 *   f    = 文件流指针（printf 内部用，这里忽略）
 * 返回值：发送的字符（C 标准要求）
 *
 * ⚠️ 这是阻塞发送！大量 printf 会拖慢主循环
 *    调试可以，正式发布建议去掉或改用 DMA
 * ============================================================ */
int fputc(int ch, FILE *f)
{
	// 通过 USART1 发送一个字节（huart1 在 usart.c 中定义，PA9/PA10 引脚）
	// 最后一个参数 0xFFFF = 最大超时，死等直到发送完成
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
	return ch;
}

/* ============================================================
 * system_init() —— 系统信息打印
 *
 * 打印项目 Logo、版本号、编译时间、芯片唯一 ID
 * 纯展示功能，不涉及硬件初始化
 *
 * 芯片编号来源：
 *   0x1FFFF7E8 / 0x1FFFF7EC / 0x1FFFF7F0
 *   这三个地址是 STM32F1 系列的系统存储区，
 *   出厂时烧录了 96 位全球唯一设备 ID（不可修改）
 *   常用于设备序列号、加密认证等场景
 * ============================================================ */
static void system_init(void)
{
	printf("\r\n");
	printf("==========================================================	\r\n");
	printf("                                                     	\r\n");
	printf("	##       ##    ##         ####  #######  ######## 	\r\n");
	printf("	##        ##  ##           ##  ##     ##    ##    	\r\n");
	printf("	##         ####            ##  ##     ##    ##    	\r\n");
	printf("	##          ##    #######  ##  ##     ##    ##    	\r\n");
	printf("	##          ##             ##  ##     ##    ##    	\r\n");
	printf("	##          ##             ##  ##     ##    ##    	\r\n");
	printf("	########    ##            ####  #######     ##    	\r\n");
	printf("                                                        \r\n");
	printf("		           IoT OneNET MQTT 	                    \r\n");
	printf("                   QwQ 紅云Obsidian                         \r\n");
	printf("==========================================================		\r\n");
	printf("  # 程序名称: IoT OneNET MQTT						\r\n");
	printf("  # 固件版本: V1.0.0                                      		\r\n");
	// __DATE__ 和 __TIME__ 是编译器预定义宏，编译时自动替换为当前日期时间
	printf("  # 编译时间: %s %s                  	 \r\n", __DATE__, __TIME__);
	// 读取芯片 96 位唯一 ID（三个 32 位字），格式化为 8 位十六进制
	printf("  # 芯片编号：%08X-%08X-%08X \r\n",
	       *(uint32_t *)0x1FFFF7E8,    // UID[31:0]  — XY 坐标
	       *(uint32_t *)0x1FFFF7EC,    // UID[63:32] — 晶圆编号
	       *(uint32_t *)0x1FFFF7F0);   // UID[95:64] — 芯片编号
	printf("==========================================================	\r\n");
	printf("\r\n");
}

/* ============================================================
 * board_init() —— 板级硬件初始化
 *
 * 执行顺序：
 *   ① 启动 PWM 输出（TIM3_CH2，控制 LED 亮度）
 *   ② 初始化 AT 指令驱动（环形缓冲区、指令表等）
 *   ③ 初始化 ESP8266（发 AT 测试通信）
 *
 * 参数 first=0 表示"软初始化"，不上电复位
 * 如果改成 first=1 则先硬件复位 ESP8266 再初始化
 * ============================================================ */
static void board_init(void)
{
	uint8_t ret;

	/* ① 启动定时器 PWM 输出
	 * htim3 在 tim.c 中由 CubeMX 生成初始化代码
	 * TIM_CHANNEL_2 对应的是 PB5 或其他引脚（取决于 CubeMX 配置）
	 * 启动后可通过 __HAL_TIM_SET_COMPARE() 调节占空比
	 */
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

	/* ② 初始化 AT 指令驱动层
	 * 内部操作：
	 *   - 初始化 at_cmd_table[] 的运行时状态
	 *   - 初始化各 LUN 的环形接收缓冲区
	 *   - 配置串口中断接收（HAL_UART_Receive_IT）
	 */
	at_init();

	/* ③ ESP8266 初始化
	 * 发 AT 指令 → 等 "OK" → 最多重试 50 次（见 at_cmd_table[ESP_AT]）
	 * ret=0 表示成功，非 0 表示超时（你之前的 50 次 TIMEOUT）
	 */
	ret = esp8266_init(0);  // 0 = 不硬复位，直接发 AT
	printf("[ESP8266] init code: %d\r\n", ret);
}

/* ============================================================
 * app_init() —— 应用层初始化（网络连接）
 *
 * 三步走：
 *   ① WiFi 连接    → 连上路由器
 *   ② MQTT 连接    → 连上 OneNET 云平台
 *   ③ 订阅主题     → 告诉平台"我要接收指令"
 *
 * 这三步有严格的先后顺序：
 *   不能没 WiFi 就连 MQTT；不能没连 MQTT 就订阅
 * ============================================================ */
static void app_init(void)
{
	uint8_t ret;

	/* ① 连接 WiFi
	 * 内部流程：
	 *   AT+CWJAP="YOUR_WIFI_SSID","YOUR_WIFI_PASSWORD"  →  等 "OK"  →  超时 8 秒
	 *   成功返回 0，失败显示错误码
	 */
	ret = esp8266_wifi_connect(wifi_info.ssid, wifi_info.password);
	printf("[ESP8266][wifi] connect code: %d\r\n", ret);

	/* ② 连接 MQTT
	 * 内部流程：
	 *   AT+MQTTUSERCFG=0,1,"dev-001","YOUR_PRODUCT_ID","密码...",0,0,""
	 *   AT+MQTTCONN=0,"mqtts.heclouds.com",1883,1
	 *   → 等 "+MQTTCONNECTED:0" → 超时 2 秒
	 */
	ret = esp8266_mqtt_connect(mqtt_info.client_id, mqtt_info.username,
	                           mqtt_info.password, mqtt_info.host, mqtt_info.port);
	printf("[ESP8266][mqtt] connect code: %d\r\n", ret);

	/* ③ 订阅属性设置主题
	 * 订阅后，平台下发指令时 ESP8266 会收到 MQTT 消息
	 * 然后通过串口传给 STM32 → AT 驱动解析 → onenet_mqtt_user_handle 回调
	 */
	ret = esp8266_mqtt_subscribe(mqtt_info.subscribe_topic_set, 0);
	printf("[ESP8266][mqtt] subscribe code: %d\r\n", ret);

	/* ④ 订阅属性上报响应主题
	 * 每次设备上报属性后，平台会回复一个响应
	 * 订阅这个主题来确认上报是否成功
	 */
	ret = esp8266_mqtt_subscribe(mqtt_info.subscribe_topic_post, 0);
	printf("[ESP8266][mqtt] subscribe code: %d\r\n", ret);
}

/* ============================================================
 * onenet_mqtt_user_handle() —— 平台下发指令的用户回调
 *
 * 触发时机：微信小程序/手机APP / Web 控制台 → OneNET 平台 → MQTT → ESP8266 → STM32
 *           数据经过 onenet_mqtt_parse_propertySet_topic() 解析后，
 *           调用此函数让用户根据 identifier（属性名）执行对应动作
 *
 * 参数：
 *   data → 解析后的属性数据，包含：
 *     .identifier  = 属性名（如 "LED1"、"JDQ"）
 *     .value       = 属性值（bool_val / f32_val / u32_val 等）
 *
 * 返回值：0=成功，其他=失败（失败时平台会收到 "user_err" 响应）
 *
 * ⚠️ 当前只处理 LED1 和 JDQ 两个属性
 *    加新属性（如风扇、蜂鸣器）在这里加 else if 分支即可
 * ============================================================ */
uint8_t onenet_mqtt_user_handle(onenet_pub_data_t *data)
{
	/* ---------- LED1 控制 ----------
	 * 映射关系（注意逻辑是反的！）：
	 *   PC13 在 Blue Pill 上连接板载 LED
	 *   板载 LED 是低电平点亮（LED 阳极接 3.3V，阴极经电阻接 PC13）
	 *
	 *   data->value.bool_val == 1 → "开灯" → PC13 输出低 → LED 亮
	 *   data->value.bool_val == 0 → "关灯" → PC13 输出高 → LED 灭
	 */
	if (strcmp(data->identifier, "LED1") == 0)   // identifier 匹配 "LED1"
	{
		// 更新本地数据缓存（onenet_data[] 用于下次上报时发送当前状态）
		onenet_data[2].value.bool_val = data->value.bool_val;

		// 控制 PC13 引脚
		// 三元运算符：bool_val==1 ? 低(亮) : 高(灭)
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
		                  (data->value.bool_val == 1) ? GPIO_PIN_RESET : GPIO_PIN_SET);
	}

	/* ---------- LED2 PWM（注释掉了，备用）---------- */
	// if (strcmp(data->identifier, "LED2") == 0)
	// {
	//     onenet_data[3].value.u32_val = data->value.u32_val;
	//     // 设置 PWM 占空比：0-999 对应 0%-100% 亮度
	//     __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, data->value.u32_val);
	// }

	/* ---------- 继电器(JDQ) 控制 ----------
	 * JDQ = 继电器，用于控制 220V 家电（风扇、灯、插座等）
	 * PA4 高电平 → 继电器吸合 → 电器通电
	 * PA4 低电平 → 继电器断开 → 电器断电
	 *
	 * 与 LED1 不同：bool_val==1 → 高电平（不需要取反）
	 */
	if (strcmp(data->identifier, "JDQ") == 0)
	{
		onenet_data[3].value.bool_val = data->value.bool_val;
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4,
		                  (data->value.bool_val == 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}

	return 0;
}

/* ============================================================
 * onenet_data_update() —— 模拟传感器数据更新
 *
 * 每 5 秒调用一次（由主循环 tick%50==0 触发）
 *
 * 当前逻辑（模拟数据，漂移到 40 后归零重新开始）：
 *   onenet_data[0] = 温度（float）
 *   onenet_data[1] = 湿度（float）
 *   每次 +0.1，到 40.0 后归零
 *
 * ⚠️ 对接真实传感器（DHT11/DS18B20）时替换这里！
 *   例如：
 *     onenet_data[0].value.f32_val = DHT11_ReadTemperature();
 *     onenet_data[1].value.f32_val = DHT11_ReadHumidity();
 * ============================================================ */
void onenet_data_update(void)
{
/*	onenet_data[0].value.f32_val += 0.1f;  // 温度模拟值每次 +0.1
	onenet_data[1].value.f32_val += 0.1f;  // 湿度模拟值每次 +0.1
*/
	/* 当温度超过 40 时归零，模拟循环数据 */
/*	if (onenet_data[0].value.f32_val > 40.0f)
	{
		onenet_data[0].value.f32_val = 0;
		onenet_data[1].value.f32_val = 0;
	}
*/
	
	MQ2_Data_t mq2_data;   /* MQ-2 传感器数据 */
	MQ2_ReadAll(&mq2_data);
	/*  ===============读取到的值===========
	 * mq2_data.ppm_methane   → 甲烷浓度
	 * mq2_data.ppm_propane   → 丙烷浓度
	 * mq2_data.ppm_smoke     → 烟雾浓度
 	 * mq2_data.alarm         → 报警状态(1=超限)
	 * ==================================== */
	uint8_t wendu, shidu;
	double guangzhao ;
    /* 读取真实温湿度，失败时保持上次的值 */
    if (DHT11_Read_Data(&wendu, &shidu) == 0)
    {
        onenet_data[0].value.f32_val = (float)wendu;  // 真实温度 → float
        onenet_data[1].value.f32_val = (float)shidu;  // 真实湿度 → float
    }
    /* 如果 DHT11_Read_Data 返回 1（失败），不改 onenet_data，上云的就是上次的有效值 */
	guangzhao=BH1750_ReadLight();
	onenet_data[4].value.f32_val = (double)guangzhao;	//光照强度
	onenet_data[5].value.f32_val = (float)mq2_data.ppm_methane;	//甲烷浓度
	onenet_data[6].value.f32_val = (float)mq2_data.ppm_propane;	//丙烷浓度
	onenet_data[7].value.f32_val = (float)mq2_data.ppm_smoke;	//烟雾浓度
	onenet_data[8].value.f32_val = (int)mq2_data.ppm_smoke;	//气体报警状态
	
}

/* ============================================================
 * app_main() —— 主函数（main.c 中调用）
 *
 * 整体流程：
 *
 * 启动阶段（只执行一次）：
 *   system_init()          打印 Logo
 *   board_init()           硬件初始化（PWM、AT 驱动、ESP8266）
 *   app_init()             网络连接（WiFi → MQTT → 订阅）
 *
 * 主循环（while(1) 每 100ms 一轮）：
 *   ┌── 检查是否有 ESP8266 发来的 MQTT 消息
 *   │   ├─ 有 → 解析主题 → 匹配 identifier → 执行用户回调
 *   │   └─ 无 → 跳过
 *   │
 *   └── 每 5 秒：
 *        ├─ 更新传感器数据
 *        └─ 通过 MQTT 上报到 OneNET 平台
 *
 * ============================================================ */

void app_main(void)
{

	uint8_t  ret  = 0;
	uint16_t tick = 0;       // 主循环计数器，每 100ms +1

	MQ2_Data_t mq2_data;   /* MQ-2 传感器数据 */
	MQ2_ReadAll(&mq2_data);
// 读取到的值：
// mq2_data.ppm_methane   → 甲烷浓度
// mq2_data.ppm_propane   → 丙烷浓度  
// mq2_data.ppm_smoke     → 烟雾浓度
// mq2_data.alarm          → 报警状态(1=超限)
	/* ===== 启动阶段 ===== */
	system_init();            // ① 打印 Logo 和系统信息
	board_init();             // ② 硬件初始化（PWM → AT → ESP8266）
	app_init();               // ③ 网络连接（WiFi → MQTT → 订阅）
	
	char xianshi1[16];			//显示1====dht11
	uint8_t wendu,shidu;

	char xianshi2[16];			//显示2====bh1750
	double guangzhao ;
	
	char xianshi3[16];			//显示3====mq-2

	/* ===== 主循环 ===== */
	while (1)
	{
		/* ===== Part A: 检查是否有 MQTT 下行数据 ===== */

		/* at_get_monitor_match_index() 检查 AT 驱动的接收监控表
		 * 如果 ESP8266 收到的数据匹配了预设的关键字
		 * （如 +MQTTSUBRECV:0,... 表示收到了订阅主题的消息），
		 * 返回匹配项的索引；否则返回 0xFF（无匹配）
		 */
		uint8_t index = at_get_monitor_match_index(AT_LUN_ESP8266);

		if (index != 0xFF)   // 有匹配 → 有平台下发的数据！
		{
			/* 获取 ESP8266 接收缓冲区中的完整 AT 响应字符串
			 * 例如：+MQTTSUBRECV:0,"$sys/.../thing/property/set",...
			 *             ...{"id":"xxx","params":{"LED1":{"value":1}}}...
			 */
			char *str = at_get_recv_buffer(AT_LUN_ESP8266);

			/* ---- 匹配第一个主题：属性设置（平台下发）---- */
			// onenet_mqtt_get_topic_data() 按 subscribe_topic_set 主题过滤，
			// 解析出 JSON 数据并存入 onenet_topic_data
			ret = onenet_mqtt_get_topic_data(
			    (char *)mqtt_info.subscribe_topic_set,
			    str, &onenet_topic_data);

			if (ret == 0)   // 匹配成功！
			{
				// 打印日志（方便调试看下发内容）
				onenet_mqtt_topic_log(&onenet_topic_data);

				// 解析 JSON，提取 identifier 和 value
				// 例如：{"id":"123","params":{"LED1":{"value":1}}} → identifier="LED1", value=1
				ret = onenet_mqtt_parse_propertySet_topic(
				    onenet_topic_data.json_data, &onenet_propertySet);

				if (ret == 0)
				{
					// 调用用户回调 → 控制硬件
					ret = onenet_mqtt_user_handle(&onenet_propertySet.data);

					// 回复平台执行结果
					if (ret == 0)
						// 成功 → 返回 200 + "user_succ"
						onenet_mqtt_response_topic(onenet_propertySet.msg_id, 200, "user_succ");
					else
						// 失败 → 返回 200 + "user_err"
						onenet_mqtt_response_topic(onenet_propertySet.msg_id, 200, "user_err");
				}
			}

			/* ---- 匹配第二个主题：属性上报响应 ---- */
			// 只打日志，不需要额外操作
			ret = onenet_mqtt_get_topic_data(
			    (char *)mqtt_info.subscribe_topic_post,
			    str, &onenet_topic_data);

			if (ret == 0)
				onenet_mqtt_topic_log(&onenet_topic_data);

			/* ★ 处理完毕，清空接收缓冲区，准备下一轮数据 */
			at_clear_recv_buffer(AT_LUN_ESP8266);
		}

		/* ===== Part B: 定时上报数据（每 5 秒） =====
		 *
		 * tick 每 100ms 加 1（HAL_Delay(100)）
		 * tick%50==0 → 每 50×100ms = 5000ms = 5 秒触发一次
		 *
		 * 如果需要改上报频率：
		 *   tick%10  = 每 1 秒
		 *   tick%100 = 每 10 秒
		 *   tick%600 = 每 60 秒（1 分钟）
		 */
		if (tick % 10 == 0)
		{
			onenet_data_update();                                // 更新传感器数据
			onenet_mqtt_publish_post_data(onenet_data, ONENET_DATA_NUM); // 上报到 OneNET
			
		// ================= 以下是为了在OLED上面显示 ===================
			
			
//			OLED_Clear();
//			DHT11_Read_Data(&wendu, &shidu);    //读取温湿度值   dht11
//			OLED_ShowString(35 ,0 ,"设备信息"  , OLED_8X16);
//			// 第2行：温度	
//			OLED_ShowString(0,  16,  "温度:", OLED_8X16);
//			OLED_ShowChar(16, 16, wendu, OLED_8X16);
//			OLED_ShowString(40, 16, "％", OLED_6X8);
//			

//			// 第3行：湿度
//			OLED_ShowString(0, 32,  "湿度:", OLED_8X16);
//			OLED_ShowChar(16, 32, shidu, OLED_8X16);
//			OLED_ShowString(40, 32, "℃", OLED_6X8);

//			// 第4行：光照
//			guangzhao = BH1750_ReadLight();     //读取光敏传感器值   bh1705
//			OLED_ShowString(0, 48,  "光照强度:", OLED_8X16);
//			OLED_ShowChar(16, 48, guangzhao, OLED_8X16);
//			OLED_ShowString(40, 48, "Lux", OLED_6X8);
//			//写入更新
//			OLED_Update();

			
	
			
//			OLED_Clear();
//			DHT11_Read_Data(&wendu,&shidu);		   //读取温湿度值   dht11
//			OLED_ShowString(35 ,0 ,"设备信息"  , OLED_8X16);
//			OLED_Printf(0, 20, OLED_8X16, "温度:%d％", wendu);
//			OLED_Printf(10, 20, OLED_8X16, "湿度:%d％", shidu);
//			guangzhao=BH1750_ReadLight();	       //读取光敏传感器值   bh1705
//			OLED_Printf(20, 20, OLED_8X16, "光照强度%.1flux", guangzhao);
//			OLED_Update();
			
			
//			DHT11_Read_Data(&wendu,&shidu);		   //读取温湿度值   dht11
//			OLED_ShowString(35 ,0     ,"设备信息"  , OLED_8X16);
//			OLED_ShowString(0  ,20    ,"温度："    ,OLED_8X16);
//			OLED_ShowChar  (7  ,20    ,wendu       ,OLED_8X16);
//			OLED_ShowString(8  ,20    ,"℃"        ,OLED_8X16);
//			OLED_ShowString(12 ,20    ,"湿度："    ,OLED_8X16);
//			OLED_ShowChar  (19 ,20    ,shidu       ,OLED_8X16);
//			OLED_ShowString(20 ,20    ,"％"        ,OLED_8X16);
//			guangzhao=BH1750_ReadLight();	       //读取光敏传感器值   bh1705
//			OLED_ShowString(0  ,40    ,"光照强度：",OLED_8X16);//光照强度
//			OLED_ShowChar  (7  ,40    ,guangzhao   ,OLED_8X16);
//			OLED_ShowString(8  ,40    ,"lux"       ,OLED_8X16);
//			OLED_Update();

			
			DHT11_Read_Data(&wendu,&shidu);		//读取温湿度值   dht11
			sprintf(xianshi1, "W:%d  S:%d    ", wendu, shidu);
			OLED_ShowString(25,16,xianshi1,OLED_8X16);//温湿度

			guangzhao=BH1750_ReadLight();	//读取光敏传感器值   bh1705
			sprintf(xianshi2, "L:%.1f lux    ", guangzhao);
			OLED_ShowString(25,32,xianshi2,OLED_8X16);//光照强度

			
			// 气体检测读取到的值：
			// mq2_data.ppm_methane   → 甲烷浓度
			// mq2_data.ppm_propane   → 丙烷浓度  
			// mq2_data.ppm_smoke     → 烟雾浓度
			// mq2_data.alarm          → 报警状态(1=超限)
			
			sprintf(xianshi3, "J:%d B:%d Y:%d     ", (uint8_t)mq2_data.ppm_methane,(uint8_t)mq2_data.ppm_propane,(uint8_t)mq2_data.ppm_smoke);
			OLED_ShowString(15,48,xianshi3,OLED_8X16);//气体检测
			OLED_Update();
			
			
			//guangzhao=BH1750_ReadLight();	//读取烟雾值   bh1705
			//sprintf(xianshi2, "Y:%.1flux", yanwu);
			//OLED_ShowString(0,48,xianshi3,OLED_8X16);//
			//OLED_Update();
			    /* --- 读取DHT11温湿度 --- */
//    DHT11_Read_Data(&wendu, &shidu);

//    // 清除温度数值区域 → 重绘 → 局部刷新
//    OLED_ClearArea(48, 0, 16, 16);             // 清旧数字(2位)
//    OLED_ShowNum(48, 0, wendu, 2, OLED_8X16);  // 写新数字
//    OLED_UpdateArea(48, 0, 16, 16);            // 只刷温度数值区

//    // 清除湿度数值区域 → 重绘 → 局部刷新
//    OLED_ClearArea(48, 16, 16, 16);
//    OLED_ShowNum(48, 16, shidu, 2, OLED_8X16);
//    OLED_UpdateArea(48, 16, 16, 16);

//    /* --- 读取BH1750光照 --- */
//    guangzhao = BH1750_ReadLight();

//    OLED_ClearArea(48, 32, 32, 16);            // 清4位数字区
//    OLED_ShowNum(48, 32, (uint32_t)guangzhao, 4, OLED_8X16);
//    OLED_UpdateArea(48, 32, 32, 16);

    /* --- 读取烟雾传感器 --- */
    // OLED_ClearArea(48, 48, 16, 16);
    // OLED_ShowNum(48, 48, smoke_val, 2, OLED_8X16);
    // OLED_UpdateArea(48, 48, 16, 16);
		}
		tick++;
		HAL_Delay(100);   // 每轮循环等 100ms，tick 自增后刚好符合 100ms 节奏

	}
}
