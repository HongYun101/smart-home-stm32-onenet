/**
  ******************************************************************************
  * @file    stm32f1xx_hal_adc.c
  * @brief   Minimal ADC HAL module implementation for STM32F1 series.
  * @note    Contains only what MQ-2 driver needs (Init, Start, Read, Calibrate).
  *
  *          【重要】第一个 include 必须是 stm32f1xx_hal.h，
  *          这样才能走正确的 include 链：
  *            stm32f1xx_hal.h → stm32f1xx_hal_conf.h → stm32f1xx_hal_def.h
  *          确保 HAL_StatusTypeDef 等基础类型在所有模块头文件之前定义。
  *          绝对不能直接 #include "stm32f1xx_hal_adc.h" 作为第一个 include！
  ******************************************************************************
  */

/* ===== 第一个 include 必须是 stm32f1xx_hal.h（跟所有 HAL .c 文件一样）===== */
#include "stm32f1xx_hal.h"

#ifdef HAL_ADC_MODULE_ENABLED

/* ======================== 内部寄存器位宏 ======================== */

/* ADC CR1 */
#define ADC_CR1_SCAN_BIT         (0x1UL << 8)   /* SCAN 位 */

/* ADC CR2 */
#define ADC_CR2_ADON             (0x1UL << 0)   /* A/D 转换器开启/关闭 */
#define ADC_CR2_CONT             (0x1UL << 1)   /* 连续转换模式 */
#define ADC_CR2_CAL              (0x1UL << 2)   /* A/D 校准 */
#define ADC_CR2_RSTCAL           (0x1UL << 3)   /* 复位校准寄存器 */
#define ADC_CR2_EXTSEL_MASK      (0x7UL << 17)  /* 外部事件选择掩码 */
#define ADC_CR2_EXTTRIG          (0x1UL << 20)  /* 外部触发转换使能 */
#define ADC_CR2_ALIGN_BIT        (0x1UL << 11)  /* 数据对齐：0=右 1=左 */
#define ADC_CR2_SWSTART          (0x1UL << 22)  /* 软件触发启动转换 */

/* ADC SR */
#define ADC_SR_EOC               (0x1UL << 1)   /* 转换结束标志 */

/* ADC SQR1 */
#define ADC_SQR1_L_MASK          (0xFUL << 20)  /* 规则序列长度掩码 */

/* ======================== 弱符号 MSP 回调 ======================== */

/**
  * @brief  ADC MSP 初始化（弱符号，用户在自己文件中重写）
  */
__weak void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    UNUSED(hadc);
}

/**
  * @brief  ADC MSP 反初始化（弱符号）
  */
__weak void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
    UNUSED(hadc);
}

/* ======================== 导出函数实现 ======================== */

/**
  * @brief  初始化 ADC（按 hadc->Init 参数配置寄存器）
  * @param  hadc  ADC 句柄
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    hadc->State = HAL_ADC_STATE_BUSY;

    /* 调用 MSP 初始化（用户可在此配置时钟和 GPIO）*/
    HAL_ADC_MspInit(hadc);

    /* 关闭 ADC，准备重新配置 */
    hadc->Instance->CR2 &= ~ADC_CR2_ADON;

    /* 配置 CR1：扫描模式 */
    hadc->Instance->CR1 &= ~ADC_CR1_SCAN_BIT;
    if (hadc->Init.ScanConvMode == ADC_SCAN_MODE_ENABLE)
    {
        hadc->Instance->CR1 |= ADC_CR1_SCAN_BIT;
    }

    /* 配置 CR2：连续模式 + 触发方式 + 对齐 */
    uint32_t cr2 = 0x00000000UL;

    if (hadc->Init.ContinuousConvMode == ENABLE)
    {
        cr2 |= ADC_CR2_CONT;
    }

    if (hadc->Init.ExternalTrigConv == ADC_SOFTWARE_START)
    {
        cr2 |= (0x7UL << 17);  /* EXTSEL = SWSTART */
    }
    else
    {
        cr2 |= ADC_CR2_EXTTRIG;
        cr2 |= (hadc->Init.ExternalTrigConv << 17) & ADC_CR2_EXTSEL_MASK;
    }

    if (hadc->Init.DataAlign == ADC_DATAALIGN_LEFT)
    {
        cr2 |= ADC_CR2_ALIGN_BIT;
    }

    hadc->Instance->CR2 = cr2;

    /* 配置规则序列长度 L[3:0] = NbrOfConversion - 1 */
    hadc->Instance->SQR1 &= ~ADC_SQR1_L_MASK;
    hadc->Instance->SQR1 |= (((hadc->Init.NbrOfConversion - 1UL) & 0xFUL) << 20);

    /* 开启 ADC */
    hadc->Instance->CR2 |= ADC_CR2_ADON;

    /* 短暂延时等待 ADC 稳定 */
    for (volatile uint32_t i = 0; i < 1000; i++);

    hadc->State = HAL_ADC_STATE_READY;
    hadc->ErrorCode = 0x0UL;

    return HAL_OK;
}

/**
  * @brief  反初始化 ADC
  * @param  hadc  ADC 句柄
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_DeInit(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    hadc->State = HAL_ADC_STATE_BUSY;

    hadc->Instance->CR2 &= ~ADC_CR2_ADON;

    HAL_ADC_MspDeInit(hadc);

    hadc->State = HAL_ADC_STATE_RESET;
    hadc->ErrorCode = 0x0UL;

    return HAL_OK;
}

/**
  * @brief  配置规则通道（通道号、Rank、采样时间）
  * @param  hadc     ADC 句柄
  * @param  sConfig  通道配置结构体
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *sConfig)
{
    uint32_t channel;

    if (hadc == NULL || hadc->Instance == NULL || sConfig == NULL)
    {
        return HAL_ERROR;
    }

    channel = sConfig->Channel;

    /* 配置采样时间 */
    if (channel <= 9)
    {
        uint32_t shift = channel * 3UL;
        hadc->Instance->SMPR2 &= ~(0x7UL << shift);
        hadc->Instance->SMPR2 |=  ((sConfig->SamplingTime & 0x7UL) << shift);
    }
    else if (channel <= 17)
    {
        uint32_t shift = (channel - 10UL) * 3UL;
        hadc->Instance->SMPR1 &= ~(0x7UL << shift);
        hadc->Instance->SMPR1 |=  ((sConfig->SamplingTime & 0x7UL) << shift);
    }

    /* 配置规则序列 Rank */
    if (sConfig->Rank <= 6)
    {
        uint32_t shift = (sConfig->Rank - 1UL) * 5UL;
        hadc->Instance->SQR3 &= ~(0x1FUL << shift);
        hadc->Instance->SQR3 |=  ((channel & 0x1FUL) << shift);
    }
    else if (sConfig->Rank <= 12)
    {
        uint32_t shift = (sConfig->Rank - 7UL) * 5UL;
        hadc->Instance->SQR2 &= ~(0x1FUL << shift);
        hadc->Instance->SQR2 |=  ((channel & 0x1FUL) << shift);
    }
    else if (sConfig->Rank <= 16)
    {
        uint32_t shift = (sConfig->Rank - 13UL) * 5UL;
        hadc->Instance->SQR1 &= ~(0x1FUL << shift);
        hadc->Instance->SQR1 |=  ((channel & 0x1FUL) << shift);
    }

    return HAL_OK;
}

/**
  * @brief  启动 ADC 转换（软件触发）
  * @param  hadc  ADC 句柄
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    hadc->Instance->SR &= ~ADC_SR_EOC;
    hadc->Instance->CR2 |= ADC_CR2_SWSTART;

    return HAL_OK;
}

/**
  * @brief  停止 ADC 转换
  * @param  hadc  ADC 句柄
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
  * @brief  等待转换完成（轮询 EOC 标志）
  * @param  hadc     ADC 句柄
  * @param  Timeout  超时时间 (ms)
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t Timeout)
{
    uint32_t tickstart = HAL_GetTick();

    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    while (!(hadc->Instance->SR & ADC_SR_EOC))
    {
        if (Timeout != 0xFFFFFFFFUL)
        {
            if ((HAL_GetTick() - tickstart) > Timeout)
            {
                hadc->State = HAL_ADC_STATE_TIMEOUT;
                return HAL_TIMEOUT;
            }
        }
    }

    return HAL_OK;
}

/**
  * @brief  读取 ADC 转换结果
  * @param  hadc  ADC 句柄
  * @retval 转换结果 (0..4095 for 12-bit right-aligned)
  */
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return 0;
    }

    return (hadc->Instance->DR & 0x00000FFFUL);
}

/**
  * @brief  ADC 校准（STM32F1 系列专用）
  *         校准流程：RSTCAL → 等待清零 → CAL → 等待清零
  * @param  hadc  ADC 句柄
  * @retval HAL 状态
  */
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL || hadc->Instance == NULL)
    {
        return HAL_ERROR;
    }

    /* 1. 复位校准寄存器 */
    hadc->Instance->CR2 |= ADC_CR2_RSTCAL;

    /* 2. 等待 RSTCAL 硬件自动清零 */
    while (hadc->Instance->CR2 & ADC_CR2_RSTCAL);

    /* 3. 启动校准 */
    hadc->Instance->CR2 |= ADC_CR2_CAL;

    /* 4. 等待 CAL 硬件自动清零 */
    while (hadc->Instance->CR2 & ADC_CR2_CAL);

    return HAL_OK;
}

#endif /* HAL_ADC_MODULE_ENABLED */
