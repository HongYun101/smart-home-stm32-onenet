# MQ2 烟雾/气体传感器 HAL 库

> 适配 **STM32F103C8T6** + **STM32CubeMX HAL库** 工程的 MQ2 传感器驱动。

## 📁 文件清单

| 文件 | 说明 |
|------|------|
| `mq2.h` | 头文件：类型定义、宏配置、函数声明 |
| `mq2.c` | 实现文件：ADC 读取、浓度换算、报警检测 |
| `mq2_example.c` | 4 个使用示例（基础轮询、标定、单一气体、OLED 显示） |

## 🔌 硬件接线

```
MQ2 模块              STM32F103C8T6
┌─────────┐          ┌────────────────────┐
│  VCC    │──────────│  5V                │
│  GND    │──────────│  GND               │
│  AOUT   │────[分压]──│  PA5 (ADC1_IN5)    │  ← 甲烷/丙烷/烟雾模拟信号
│  DOUT   │──────────│  PA6               │  ← 数字报警
└─────────┘          └────────────────────┘
```

### ⚠️ 5V→3.3V 分压电路（必加！）

MQ2 模块 VCC=5V，AOUT 输出最高 5V，而 STM32 ADC 仅能承受 3.3V：

```
  AOUT ●─── 10kΩ ───┬─────● PA5  (ADC输入)
                     │
                    20kΩ
                     │
                    GND
```

分压比 2/3：Vadc = Vaout × 0.667
STM32 读取 Vadc 后, 还原 Vaout = Vadc / 0.667
代码中如使用本库默认 VREF=3300mV，**分压影响已在浓度换算中保持比例**（最终 ppm 换算用的是 Rs/R0 比值, 不依赖绝对电压），所以**不用额外修改**。

## 🛠️ STM32CubeMX 配置

### 1. ADC1 配置
- Channel 5 (PA5) 使能
- Single Conversion Mode
- 12-Bit Resolution
- 不开 DMA / 不开中断 (本库用轮询方式)

### 2. GPIO PA6 配置 (DOUT)
- Input Mode
- Pull-up (大多数模块 DOUT 是开漏输出)

### 3. USART1 (调试用)
- 115200 波特率 (可选, 用于 printf 调试)

## 📋 集成步骤

1. **复制文件**：将 `mq2.h` 和 `mq2.c` 复制到工程 `Core/Inc/` 和 `Core/Src/`。
2. **添加源文件**：在 Keil / STM32CubeIDE 中添加 `mq2.c` 到工程。
3. **包含头文件**：`#include "mq2.h"`
4. **确认 ADC 句柄**：在 `mq2.h` 中确认 `MQ2_ADC_HANDLE` 与你工程一致（默认 `hadc1`）。
5. **修改引脚**（如果不同）：修改 `mq2.h` 顶部的 `MQ2_DOUT_PORT` / `MQ2_DOUT_PIN`。

## 🚀 快速上手

```c
#include "mq2.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART1_UART_Init();

    // 1. 初始化 (记录预热时间)
    MQ2_Init();

    // 2. 首次使用: 在清洁空气中标定 R0
    //    标定后用 MQ2_GetR0() 获取并硬编码, 后续跳过标定
    // MQ2_WaitWarmup();
    // MQ2_Calibrate();

    // 3. 已标定过: 直接设置R0 (如 6.34 kΩ)
    // MQ2_SetR0(6.34f);

    // 4. 等待预热完成
    MQ2_WaitWarmup();

    while (1)
    {
        MQ2_DataTypeDef data;
        if (MQ2_ReadAll(&data) == MQ2_OK)
        {
            printf("LPG=%.1fppm  CO=%.1fppm  Smoke=%.1fppm\r\n",
                   data.ppm_LPG, data.ppm_CO, data.ppm_Smoke);
        }
        HAL_Delay(1000);
    }
}
```

## 📐 浓度换算原理

MQ2 内部是一个**随浓度变化的可变电阻 Rs**，与模块上**负载电阻 RL** 组成分压器：

```
        5V
         │
         ┌┴┐
         │ │   Rs   (传感器电阻, 随浓度变化)
         └┬┘
         ├─── AOUT → ADC
         ┌┴┐
         │ │   RL   (负载电阻, 模块上1kΩ)
         └┬┘
         │
        GND
```

**反推 Rs：**

```
Rs = RL × (Vcc - Vadc) / Vadc
```

**查曲线得 ppm**（数据手册 Fig.3 对数坐标拟合）：

```
ppm = A × (Rs/R0)^B
```

| 气体 | A 系数 | B 指数 |
|------|--------|--------|
| LPG 液化气 | 574.25 | -2.222 |
| CO 一氧化碳 | 36974 | -3.109 |
| 烟雾 Smoke | 3616.1 | -2.675 |

⚠️ 系数基于典型 MQ2 模块的标定曲线，**实际值会因传感器个体差异有偏差**，请用标准气体进行精确标定。

## 🔧 主要 API

| 函数 | 说明 |
|------|------|
| `MQ2_Init()` | 初始化传感器（记录预热时间） |
| `MQ2_Calibrate()` | 在清洁空气中标定 R0 |
| `MQ2_SetR0(float r0)` | 手动设置 R0（跳过标定） |
| `MQ2_GetR0()` | 获取当前 R0 值 |
| `MQ2_WaitWarmup()` | **阻塞**等待 20s 预热完毕 |
| `MQ2_IsWarmedUp()` | **非阻塞**检查预热状态 |
| `MQ2_ReadADC(uint16_t*)` | 读取 ADC 原始值（多次采样均值） |
| `MQ2_ReadVoltage(uint16_t*)` | 读取电压 (mV) |
| `MQ2_ReadRs(float*)` | 读取传感器电阻 (kΩ) |
| `MQ2_GetPPM(MQ2_GasTypeDef, float*)` | 读取指定气体 ppm 浓度 |
| `MQ2_ReadDigitalAlarm()` | 读取数字报警引脚状态 |
| `MQ2_ReadAll(MQ2_DataTypeDef*)` | **一次性读取所有数据**（推荐） |

## ⚠️ 注意事项

1. **预热时间 20s**：上电后必须等 20s 数据才稳定，库中已处理。
2. **首次使用必须标定 R0**：在清洁空气环境下调用 `MQ2_Calibrate()`，将得到的 R0 值固化到代码。
3. **5V 电平兼容**：必须加分压电路保护 ADC 引脚。
4. **采样次数可调**：`MQ2_SAMPLE_COUNT` 默认 5 次，可改 3~10 平衡精度和速度。
5. **AOUT 输出与浓度关系**：浓度↑ → Rs↓ → Vout↑ → ADC 读数↑。
6. **DOUT 极性**：默认 `MQ2_ALARM_ACTIVE_LOW = 1`（低电平报警），如模块反向请设为 0。

## 🐛 故障排查

| 现象 | 原因 | 解决 |
|------|------|------|
| ADC 值始终 0 | AOUT 接错或分压电阻断路 | 检查分压电路、确认 PA0 |
| ADC 值始终 4095 | AOUT 直接 5V 输入烧毁或量程错 | 加分压 / 换 ADC 通道 |
| Rs 一直是固定值 | Vref 配错 / ADC 通道错 | 用万用表量 AOUT 是否随气体变化 |
| ppm 始终为 0 或 Inf | R0 未标定 / 设为 0 | 调用 `MQ2_Calibrate()` |
| 数字报警不响应 | DOUT 引脚错 / 上拉缺失 | 用万用表量 DOUT 是否在低/高电平间翻转 |

---
✅ 该库针对 STM32F103C8T6 HAL 编写，与你毕设项目（STM32F103C8T6 + BH1750 + DHT11 + OLED）的整体风格保持一致，可直接集成。
