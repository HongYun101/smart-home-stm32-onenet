# 智能家居环境监测与远程控制系统

> STM32F103C8T6 + ESP8266 + OneNET 云平台 + 微信小程序 · 毕业设计

MCU 采集温湿度、可燃气体与光照数据，经 ESP8266 通过 MQTT 将数据上报至 OneNET 物模型；微信小程序实时展示数据并提供远程开关控制，形成「端—云—小程序」完整闭环。

## 功能一览

| 模块 | 实现 |
|---|---|
| 环境采集 | DHT11 温湿度、MQ-2 可燃气体（甲烷/丙烷/烟雾）、BH1750 光照强度 |
| 本地显示 | OLED 实时显示各项数据与设备状态 |
| 云端通信 | ESP8266 AT 指令建立 MQTT 连接，OneNET 物模型属性上报 / 属性设置双向收发 |
| 执行控制 | LED1 开关、LED2 亮度（TIM3_CH2 PWM）、继电器 JDQ 开关 |
| 危险告警 | MQ-2 浓度超阈值置位 `weixian` 并在小程序端提示 |
| 小程序 | 实时数据看板、历史曲线、设备在线状态、远程开关、本地天气（Open-Meteo） |

## 系统架构

```
┌──────────┐  单总线   ┌─────────────────┐  USART3   ┌──────────┐  MQTT/TCP   ┌──────────┐
│  DHT11   │──────────▶│                 │──────────▶│ ESP8266  │────────────▶│  OneNET  │
├──────────┤   I2C     │ STM32F103C8T6   │  AT 指令   │ (AT 固件)│  1883 端口  │  IoT 平台 │
│  BH1750  │──────────▶│                 │◀──────────│          │◀────────────│          │
├──────────┤   ADC     │  HAL + 自研 AT  │           └──────────┘             └────┬─────┘
│   MQ-2   │──────────▶│  驱动 / 环形缓冲 │                                        │ HTTP API
├──────────┤   I2C     │                 │                                        ▼
│  OLED    │◀─────────▶│                 │                                  ┌──────────┐
├──────────┤   GPIO    │                 │                                  │ 微信小程序│
│ 继电器/LED│◀─────────▶│                 │                                  └──────────┘
└──────────┘           └─────────────────┘
```

数据流：

- **上行**：传感器采样 → `onenet_data[]` → JSON 封装 → `$sys/{pid}/{dev}/thing/property/post` → 平台
- **下行**：平台 `thing/property/set` → ESP8266 → 串口中断 → AT 驱动解析 → 主题匹配 → 用户回调 → 执行动作 → `set_reply` 应答

## 硬件连接

| 模块 | 引脚 | 说明 |
|---|---|---|
| DHT11 DATA | PB12 | 单总线，时序由软件模拟 |
| BH1750 SCL / SDA | PB6 / PB7 | 软件 I2C |
| OLED SCL / SDA | PB8 / PB9 | 软件 I2C，0.96 寸 128×64 |
| MQ-2 AO | PA（ADC1 通道） | 见 `mcu-code/HARDWARE/MQ2_HAL/mq2.c` |
| 继电器 JDQ | PA4 | `RELAY_Pin` |
| LED1 | PC13 | 板载指示灯 |
| LED2 | TIM3_CH2 (PB5) | PWM 调光，见 `Project.ioc` |
| ESP8266 | USART3：PB10(TX) / PB11(RX) | AT 固件，115200 |
| 调试串口 | USART1：PA9(TX) / PA10(RX) | `printf` 重定向输出 |

## 目录结构

```
Test/
├── mcu-code/                     STM32 固件（Keil MDK 工程）
│   ├── App/
│   │   ├── app_main.c            应用主逻辑：WiFi→MQTT→订阅、数据上报、指令执行
│   │   └── app_onenet_mqtt.c     OneNET 物模型协议层：JSON 封装/解析、主题管理
│   ├── HARDWARE/                 传感器与显示驱动
│   │   ├── DHT11/                温湿度（单总线时序模拟）
│   │   ├── BH1750/               光照（软件 I2C）
│   │   ├── MQ2_HAL/              可燃气体（ADC 采样 + 阈值判定）
│   │   └── OLED/                 0.96 寸 OLED 显示 + 字库
│   ├── Middlewares/EmbATlink/    AT 指令框架：指令表、环形缓冲、超时重试
│   │   └── esp8266.c             WiFi 连接 / MQTT 连接 / 订阅发布的 AT 封装
│   ├── Drivers/                  STM32F1xx HAL 驱动（ST 官方）
│   └── Project/                  CubeMX 生成的工程与 MDK-ARM 编译配置
├── 微信小程序/                   小程序端（原生框架）
│   ├── pages/index/              数据看板 + 远程控制页
│   └── utils/mqtt.js             小程序 MQTT 客户端库
└── OneNet.txt                    OneNET 平台接入参数与 AT 指令调试记录
```

## OneNET 物模型

设备属性（identifier 与固件上报、小程序展示一一对应）：

| identifier | 含义 | 类型 |
|---|---|---|
| `wendu` | 温度 ℃ | float |
| `shidu` | 湿度 %RH | float |
| `jiawan` | 甲烷浓度 | int32 |
| `bingwan` | 丙烷浓度 | int32 |
| `yanwu` | 烟雾浓度 | int32 |
| `LightLux` | 光照强度 lux | int32 |
| `weixian` | 危险状态 0=正常 1=危险 | int32 |
| `LED1` | 指示灯开关 | bool |
| `LED2` | 亮度 0–100 | int32 |
| `JDQ` | 继电器开关 | bool |

使用的 MQTT Topic（物模型标准格式）：

```
$sys/{产品ID}/{设备名}/thing/property/post          设备 → 平台：属性上报
$sys/{产品ID}/{设备名}/thing/property/post/reply    平台 → 设备：上报应答
$sys/{产品ID}/{设备名}/thing/property/set           平台 → 设备：属性设置（下发指令）
$sys/{产品ID}/{设备名}/thing/property/set_reply     设备 → 平台：设置应答
```

## 快速上手

### 1. 配置设备端参数

`mcu-code/App/app_main.c`：

```c
wifi_info_t wifi_info = {
    .ssid     = "YOUR_WIFI_SSID",       // 仅支持 2.4GHz，5G 连不上
    .password = "YOUR_WIFI_PASSWORD",
};

mqtt_info_t mqtt_info = {
    .client_id = "dev-001",             // OneNET 平台注册的设备名
    .username  = "YOUR_PRODUCT_ID",     // 产品 ID
    .password  = "YOUR_DEVICE_TOKEN",   // 平台生成的 MQTT Token
    ...
};
```

> ⚠️ 本仓库中的产品 ID、设备密钥、MQTT Token、AccessKey、WiFi 密码均已替换为占位符。Token 生成方法见 `OneNet.txt`（`et` 为过期时间戳，过期后需重新生成）。

### 2. 编译烧录

用 Keil MDK 打开 `mcu-code/Project/MDK-ARM/Project.uvprojx`，编译后用 ST-Link 烧录；上电后 USART1（115200）可看到初始化日志与芯片 UID。

### 3. 小程序配置

`微信小程序/pages/index/index.js` 顶部：

```js
const CONFIG = {
  AUTH_INFO:  "YOUR_TOKEN",        // 账号级 API Token
  PRODUCT_ID: "YOUR_PRODUCT_ID",
  DEVICE_NAME: "dev-001",
  API_BASE_URL: "https://iot-api.heclouds.com",
};
```

微信开发者工具导入 `微信小程序/` 目录即可运行（仓库内 `project.config.json` 的 appid 为 `touristappid` 游客模式，真机调试/上传时改填自己的小程序 AppID）。同时需在小程序后台配置 `iot-api.heclouds.com` 与 `api.open-meteo.com` 的 request 合法域名。

## 实现要点

- **AT 指令框架**：指令表 + 环形缓冲区 + 超时重试，把 ESP8266 的异步返回（`OK` / `ERROR` / `+MQTTxxx`）统一成同步调用，避免裸写 `HAL_UART_Receive` 时的丢字节问题。
- **主题匹配分发**：串口中断只负责收数据，主循环按 Topic 解析后查表分发到对应处理函数，采集与通信互不阻塞。
- **属性上报节流**：传感器变化才上报，避免刷爆平台免费额度；上报失败由 `post/reply` 应答确认后重试。
- **下发指令幂等**：`set` 消息执行后统一回 `set_reply`，小程序侧用本地状态先更新、失败回滚。

## 已知限制

- ESP8266 AT 固件仅支持 2.4GHz WiFi。
- 源码注释为 GBK 编码，Keil 中正常显示；GitHub 网页查看可能显示乱码，可自行转为 UTF-8。
- OneNET 免费版有调用次数与存储限制，历史曲线仅保留近 7 天。

## 参考

- [OneNET 物联网平台文档](https://open.iot.10086.cn/doc/)
- [Open-Meteo 免费天气 API](https://open-meteo.com/en/docs)
- STM32F1xx HAL 驱动库（ST 官方，见 `mcu-code/Project/Drivers` 内 LICENSE）

---

作者：赵宏俊 · 宜宾职业技术学院 · 物联网应用技术
