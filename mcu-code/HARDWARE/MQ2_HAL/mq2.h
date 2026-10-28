/**
 * @file    mq2.h
 * @brief   MQ-2 烟雾/可燃气体传感器驱动（寄存器版，不依赖 HAL ADC 库）
 * @note    引脚配置：
 *            - VCC  -> 5V (传感器供电)
 *            - GND  -> GND
 *            - AOUT -> PA5 (ADC1_IN5)   ← 检测甲烷/丙烷/烟雾 模拟信号
 *            - DOUT -> PA6 (GPIO_INPUT) ← 数字报警（已改为 ppm 阈值判断，PA6 可选）
 *
 *          【重要】本驱动 ADC 部分**直接操作寄存器**
 *          不依赖 stm32f1xx_hal_adc.h，仅依赖 stm32f1xx_hal.h（提供 HAL_Delay）
 *          只需在 main() 中先调用 HAL_Init() + SystemClock_Config() 即可。
 */

#ifndef __MQ2_H
#define __MQ2_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 依赖：stm32f1xx.h（芯片寄存器定义，所有 F1 工程都应有）
 *       或 stm32f1xx_hal.h（HAL 基础头文件）
 * 本驱动 ADC 部分直接操作寄存器，不依赖 stm32f1xx_hal_adc.h
 */
#include "stm32f1xx_hal.h"   /* 提供 HAL_Delay() 等基础 HAL 函数 */

/* ======================== 引脚与硬件参数定义 ======================== */

/* ======================== 引脚定义 ======================== */
/* PA5 -> ADC1_IN5 模拟输入（AOUT）*/
/* PA6 -> GPIO_INPUT（DOUT，可选，当前报警逻辑基于 ppm 阈值判断）*/

/* ======================== 传感器参数 ======================== */

/*
 * MQ-2 传感器特性：
 *   - 预热时间：20~30 秒（首次上电需等待预热）
 *   - 检测范围：300~10000 ppm（甲烷/丙烷/烟雾等）
 *   - 供电电压：5V
 *   - 输出模拟电压：0~5V
 *
 * RL 值：模块板载负载电阻，常见为 5kΩ 或 10kΩ
 * R0 值：在洁净空气中传感器的电阻值（需标定获取，典型值约 10kΩ）
 */

#define MQ2_RL_VALUE        5.0f            /* 负载电阻 RL (kΩ)，按模块实际值修改 */
#define MQ2_RO_VALUE        10.0f           /* 洁净空气中传感器电阻 R0 (kΩ)，需标定 */
#define MQ2_VREF            3.3f            /* ADC 参考电压 (V) */
#define MQ2_ADC_RESOLUTION  4096.0f         /* 12-bit ADC 分辨率 */
#define MQ2_WARMUP_MS       30000           /* 预热时间 30s */
#define MQ2_SAMPLE_COUNT    10              /* 多次采样取平均的样本数 */
#define MQ2_SAMPLE_INTERVAL 5               /* 采样间隔 (ms) */

/* 报警阈值 (ppm) — 任意一种气体超过对应阈值即触发报警 */
#define MQ2_ALARM_METHANE   1000.0f    /* 甲烷报警阈值 (ppm)，天然气泄漏 */
#define MQ2_ALARM_PROPANE   1000.0f    /* 丙烷报警阈值 (ppm)，液化石油气泄漏 */
#define MQ2_ALARM_SMOKE      300.0f    /* 烟雾报警阈值 (ppm)，火灾预警 */

/* 预热显示回调函数类型：每秒调用一次，传入剩余秒数 */
typedef void (*MQ2_WarmupCallback)(uint8_t remain_sec);

/* ======================== 数据结构 ======================== */

/**
 * @brief MQ-2 传感器数据结构
 *
 * MQ-2 可检测多种可燃气体，以下三个 ppm 值由同一 Rs/R0 比值
 * 通过不同拟合曲线换算得到：
 *   - 丙烷(LPG)：MQ-2 最敏感，精度最高
 *   - 甲烷(天然气)：灵敏度中等，仅供参考
 *   - 烟雾：适用于火灾预警
 */
typedef struct {
    uint16_t adc_raw;           /* ADC 原始值 (0~4095) */
    float    voltage;           /* AOUT 模拟电压 (V) */
    float    rs;                /* 传感器电阻 Rs (kΩ) */
    float    ratio;             /* Rs/R0 比值（洁净空气中 ≈ 9.9） */
    float    ppm_methane;       /* 甲烷(天然气) 浓度 (ppm) */
    float    ppm_propane;       /* 丙烷(LPG/液化石油气) 浓度 (ppm) */
    float    ppm_smoke;         /* 烟雾浓度 (ppm) */
    uint8_t  alarm;             /* 数字报警: 1=超限报警, 0=正常 */
} MQ2_Data_t;

/* ======================== 函数声明 ======================== */

/**
 * @brief  MQ-2 传感器预热（带回调，用于 OLED 显示倒计时）
 *         预热期间每秒调用一次回调函数，传入剩余秒数
 *         预热完成后回调参数为 0
 *
 *         用法示例：
 *           void warmup_display(uint8_t sec) {
 *               char buf[20];
 *               sprintf(buf, "Warmup:%2ds ", sec);
 *               OLED_ShowString(0, 48, buf, OLED_8X16);
 *               OLED_Update();
 *           }
 *           MQ2_Warmup(warmup_display);
 *
 * @param  callback  预热回调函数，传 NULL 则无显示（等同 HAL_Delay）
 */
void MQ2_Warmup(MQ2_WarmupCallback callback);

/**
 * @brief  初始化 MQ-2 传感器（GPIO + ADC + 校准）
 *         无需 CubeMX，驱动内部完成全部初始化
 *         调用前需确保已执行 HAL_Init() + SystemClock_Config()
 * @retval 0: 成功, 其他: 失败
 */
int MQ2_Init(void);

/**
 * @brief  读取 MQ-2 传感器全部数据（模拟+数字）
 * @param  data  输出数据结构指针
 * @retval 0: 成功
 */
int MQ2_ReadAll(MQ2_Data_t *data);

/**
 * @brief  读取 ADC 模拟值（多次采样取平均）
 * @retval ADC 平均值 (0~4095)
 */
uint16_t MQ2_ReadADC(void);

/**
 * @brief  判断危险气体浓度是否超标（基于 ppm 阈值）
 *         任意一种气体浓度超过对应阈值即报警
 *         阈值可在 mq2.h 中修改：MQ2_ALARM_METHANE / MQ2_ALARM_PROPANE / MQ2_ALARM_SMOKE
 * @param  data  已读取的传感器数据指针（需先调用 MQ2_ReadAll）
 * @retval 1: 报警(浓度超标), 0: 正常
 */
uint8_t MQ2_ReadAlarm(MQ2_Data_t *data);

/**
 * @brief  标定 R0 值（在洁净空气中调用）
 *         传感器预热完成后，在洁净空气中调用此函数
 *         函数会返回当前 R0 值，写入 MQ2_RO_VALUE 宏即可
 * @retval 标定得到的 R0 值 (kΩ)
 */
float MQ2_CalibrateR0(void);

/**
 * @brief  将 Rs/R0 比值转换为气体浓度 (ppm)
 * @param  ratio  Rs/R0 比值
 * @param  gas    气体类型: 0=甲烷, 1=丙烷, 2=烟雾
 * @retval 估算浓度 (ppm)
 */
float MQ2_RatioToPPM(float ratio, uint8_t gas);

#ifdef __cplusplus
}
#endif

#endif /* __MQ2_H */
