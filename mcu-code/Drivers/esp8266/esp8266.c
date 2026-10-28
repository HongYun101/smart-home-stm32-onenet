#include "esp8266.h"
#include "at_driver.h"

/**
 * @brief   初始化ESP8266模块
 * @param   is_frist 是否是第一次初始化
 * @retval  0:成功，其他:失败
 */
/**
 * @brief  ESP8266 WiFi模块初始化函数
 * @param  first: 标记是否为首次上电初始化
 *                1 = 首次初始化（执行恢复出厂设置）
 *                0 = 非首次（仅执行软复位）
 * @retval uint8_t: 返回执行结果
 *                0 = 初始化成功
 *                非0 = 失败，返回值为**出错步骤编号**
 */

uint8_t esp8266_init(uint8_t first)
{
    char *p_recv = NULL;        // 定义接收数据指针，用于存放模块返回数据（代码内暂未实际使用）
    uint8_t index = 0;          // 步骤编号，记录当前执行到第几步，出错时用于定位

    index++;                    // 第1步：发送基础AT指令，检测模块是否正常响应
    // 发送AT指令，测试模块通信是否正常；应答正常则继续，失败跳转到error
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_AT, "") != 0)
        goto error;

    index++;                    // 第2步：根据是否首次初始化，选择复位方式
    if (first == 1)             // 如果是【首次上电初始化】
    {
        // 发送指令：恢复ESP8266出厂设置
        if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_RESTORE, "") != 0)
            goto error;
    }
    else                        // 非首次初始化
    {
        // 发送指令：执行ESP8266软复位（重启模块）
        if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_RST, "") != 0)
            goto error;
    }

    index++;                    // 第3步：复位/恢复出厂后，再次发送AT指令重测通信
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_AT, "") != 0)
        goto error;

    index++;                    // 第4步：关闭模块回显（ATE0）
    // 发送ATE0指令：关闭指令回显，避免串口收到重复指令数据，简化解析
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_ATE, "0") != 0)
        goto error;

    return 0;                   // 所有步骤执行完毕，初始化成功，返回0

// 统一错误处理入口
error:
    // 打印错误日志：输出出错步骤编号 + 模块返回数据
    ESP_LOG("[ESP8266][ERROR] init err:%d, recv: %s \r\n", index, p_recv);
    return index;               // 返回出错的步骤编号，供上层函数判断故障位置
}

/**
 * @brief   连接WIFI
 * @param   ssid:WIFI名称
 * @param   pwd:WIFI密码
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_wifi_connect(char *ssid, char *pwd)
{
    char *p_recv = NULL;
    uint8_t index = 0;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_WIFI_SET_MODE, "1") != 0)
        goto error;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_WIFI_CONNECT, "\"%s\",\"%s\"", ssid, pwd) != 0)
        goto error;

    return 0;

error:
    ESP_LOG("[ESP8266][ERROR] wifi_connect err:%d, recv: %s \r\n", index, p_recv);
    return index;
}

/**
 * @brief   连接MQTT
 * @param   client_id:MQTT客户端ID
 * @param   username:MQTT用户名
 * @param   password:MQTT密码
 * @param   host:MQTT服务器地址
 * @param   port:MQTT服务器端口
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_connect(char *client_id, char *username, char *password, char *host, uint16_t port)
{
    char *p_recv = NULL;
    uint8_t index = 0;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_MQTT_SET_INFO,
                                    "0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                                    client_id, username, password) != 0)
        goto error;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_MQTT_CONNECT, "0,\"%s\",%d,1", host, port) != 0)
        goto error;

    return 0;

error:
    ESP_LOG("[ESP8266][ERROR] mqtt_connect err:%d, recv: %s \r\n", index, p_recv);
    return index;
}

/**
 * @brief   订阅MQTT
 * @param   topic:MQTT主题
 * @param   qos:QOS等级
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_subscribe(char *topic, uint8_t qos)
{
    char *p_recv = NULL;
    uint8_t index = 0;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_MQTT_SUBSCRIBE, "0,\"%s\",%d", topic, qos) != 0)
        goto error;

    return 0;

error:
    ESP_LOG("[ESP8266][ERROR] mqtt_subscribe err:%d, recv: %s \r\n", index, p_recv);
    return index;
}

/**
 * @brief   发布MQTT
 * @param   topic:MQTT主题
 * @param   data:数据
 * @param   len:数据长度
 * @param   qos:QOS等级
 * @param   retain:是否保留
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_publish(char *topic, char *data, uint16_t len, uint8_t qos, uint8_t retain)
{
    char *p_recv = NULL;
    uint8_t index = 0;

    index++;
    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_MQTT_PUBLISH_RAW, "0,\"%s\",%d,%d,%d", topic, len, qos, retain) != 0)
        goto error;

    if (at_cmd_format_send_and_recv(AT_LUN_ESP8266, NULL, ESP_MQTT_PUBLISH_DATA, "%s", data) != 0)
        goto error;

    return 0;

error:
    ESP_LOG("[ESP8266][ERROR] mqtt_publish err:%d, recv: %s \r\n", index, p_recv);
    return index;
}
