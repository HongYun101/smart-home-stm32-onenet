/**
  ******************************************************************************
  * @file    stm32f1xx_hal_adc.h
  * @brief   Header file of ADC HAL module for STM32F1 series.
  * @note    Minimal version — only what MQ-2 driver needs.
  *          All base types (HAL_StatusTypeDef, HAL_LockTypeDef, etc.)
  *          come from stm32f1xx_hal_def.h, NOT redefined here.
  *
  *          【重要】此文件由 stm32f1xx_hal_conf.h 通过
  *          #define HAL_ADC_MODULE_ENABLED 条件包含。
  *          绝对不能在 .c 文件中直接作为第一个 include 使用！
  ******************************************************************************
  */

#ifndef __STM32F1xx_HAL_ADC_H
#define __STM32F1xx_HAL_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* 依赖基础 HAL 定义（HAL_StatusTypeDef, HAL_LockTypeDef 等）
 * 此头文件由 stm32f1xx_hal_conf.h 包含，
 * stm32f1xx_hal_def.h 在 stm32f1xx_hal_conf.h 中已经先被包含 */
#include "stm32f1xx_hal_def.h"

/* ======================== ADC 状态枚举 ======================== */

typedef enum
{
    HAL_ADC_STATE_RESET       = 0x00U,
    HAL_ADC_STATE_READY       = 0x01U,
    HAL_ADC_STATE_BUSY        = 0x02U,
    HAL_ADC_STATE_BUSY_REG    = 0x04U,
    HAL_ADC_STATE_TIMEOUT     = 0x40U,
    HAL_ADC_STATE_ERROR       = 0x80U
} HAL_ADC_StateTypeDef;

/* ======================== ADC 初始化结构体 ======================== */

typedef struct
{
    uint32_t ScanConvMode;          /*!< ADC_SCAN_MODE_DISABLE / ADC_SCAN_MODE_ENABLE */
    uint32_t ContinuousConvMode;    /*!< ENABLE or DISABLE */
    uint32_t DiscontinuousConvMode; /*!< ENABLE or DISABLE */
    uint32_t ExternalTrigConv;      /*!< ADC_SOFTWARE_START or trigger source */
    uint32_t DataAlign;             /*!< ADC_DATAALIGN_RIGHT / ADC_DATAALIGN_LEFT */
    uint32_t NbrOfConversion;       /*!< Number of conversions (1..16) */
} ADC_InitTypeDef;

/* ======================== ADC 通道配置结构体 ======================== */

typedef struct
{
    uint32_t Channel;       /*!< ADC_CHANNEL_0 .. ADC_CHANNEL_17 */
    uint32_t Rank;          /*!< ADC_REGULAR_RANK_1 .. ADC_REGULAR_RANK_16 */
    uint32_t SamplingTime;  /*!< ADC_SAMPLETIME_xxxCYCLES */
} ADC_ChannelConfTypeDef;

/* ======================== ADC 句柄结构体 ======================== */

typedef struct __ADC_HandleTypeDef
{
    ADC_TypeDef*            Instance;  /*!< ADC registers base address */
    ADC_InitTypeDef         Init;      /*!< ADC initialization parameters */
    HAL_LockTypeDef         Lock;      /*!< Locking object (from hal_def.h) */
    __IO HAL_ADC_StateTypeDef State;   /*!< ADC state */
    __IO uint32_t           ErrorCode; /*!< ADC error code */
} ADC_HandleTypeDef;

/* ======================== ADC 扫描模式 ======================== */

#define ADC_SCAN_MODE_DISABLE     (0x00000000UL)
#define ADC_SCAN_MODE_ENABLE      (0x00000001UL)

/* ======================== ADC 外部触发 / 软件触发 ======================== */

#define ADC_SOFTWARE_START        (0x0000000FUL)

/* ======================== ADC 数据对齐 ======================== */

#define ADC_DATAALIGN_RIGHT       (0x00000000UL)
#define ADC_DATAALIGN_LEFT        (0x00000001UL)

/* ======================== ADC 通道号 ======================== */

#define ADC_CHANNEL_0             (0x00000000UL)
#define ADC_CHANNEL_1             (0x00000001UL)
#define ADC_CHANNEL_2             (0x00000002UL)
#define ADC_CHANNEL_3             (0x00000003UL)
#define ADC_CHANNEL_4             (0x00000004UL)
#define ADC_CHANNEL_5             (0x00000005UL)  /*!< PA5 = ADC1_IN5 */
#define ADC_CHANNEL_6             (0x00000006UL)
#define ADC_CHANNEL_7             (0x00000007UL)
#define ADC_CHANNEL_8             (0x00000008UL)
#define ADC_CHANNEL_9             (0x00000009UL)
#define ADC_CHANNEL_10            (0x0000000AUL)
#define ADC_CHANNEL_11            (0x0000000BUL)
#define ADC_CHANNEL_12            (0x0000000CUL)
#define ADC_CHANNEL_13            (0x0000000DUL)
#define ADC_CHANNEL_14            (0x0000000EUL)
#define ADC_CHANNEL_15            (0x0000000FUL)
#define ADC_CHANNEL_16            (0x00000010UL)
#define ADC_CHANNEL_17            (0x00000011UL)

/* ======================== ADC 规则序列 Rank ======================== */

#define ADC_REGULAR_RANK_1        (0x00000001UL)
#define ADC_REGULAR_RANK_2        (0x00000002UL)
#define ADC_REGULAR_RANK_3        (0x00000003UL)
#define ADC_REGULAR_RANK_4        (0x00000004UL)
#define ADC_REGULAR_RANK_5        (0x00000005UL)
#define ADC_REGULAR_RANK_6        (0x00000006UL)
#define ADC_REGULAR_RANK_7        (0x00000007UL)
#define ADC_REGULAR_RANK_8        (0x00000008UL)
#define ADC_REGULAR_RANK_9        (0x00000009UL)
#define ADC_REGULAR_RANK_10       (0x0000000AUL)
#define ADC_REGULAR_RANK_11       (0x0000000BUL)
#define ADC_REGULAR_RANK_12       (0x0000000CUL)
#define ADC_REGULAR_RANK_13       (0x0000000DUL)
#define ADC_REGULAR_RANK_14       (0x0000000EUL)
#define ADC_REGULAR_RANK_15       (0x0000000FUL)
#define ADC_REGULAR_RANK_16       (0x00000010UL)

/* ======================== ADC 采样时间 ======================== */

#define ADC_SAMPLETIME_1CYCLES_5  (0x00000000UL)
#define ADC_SAMPLETIME_7CYCLES_5  (0x00000001UL)
#define ADC_SAMPLETIME_13CYCLES_5 (0x00000002UL)
#define ADC_SAMPLETIME_28CYCLES_5 (0x00000003UL)
#define ADC_SAMPLETIME_41CYCLES_5 (0x00000004UL)
#define ADC_SAMPLETIME_55CYCLES_5 (0x00000005UL)
#define ADC_SAMPLETIME_71CYCLES_5 (0x00000006UL)
#define ADC_SAMPLETIME_239CYCLES_5 (0x00000007UL)

/* ======================== 函数声明 ======================== */

/* 初始化 / 反初始化 */
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_DeInit(ADC_HandleTypeDef *hadc);
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc);
void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc);

/* 通道配置 */
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *sConfig);

/* 转换控制 */
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t Timeout);
uint32_t          HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);

/* 校准（F1 专用）*/
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *hadc);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_HAL_ADC_H */
