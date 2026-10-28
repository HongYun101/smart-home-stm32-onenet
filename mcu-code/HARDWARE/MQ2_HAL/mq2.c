/**
 * @file      mq2.c
 * @brief     MQ-2 烟雾/可燃气体传感器驱动（寄存器版，不依赖 HAL ADC 库）
 * @hardware  STM32F103C8T6
 *             - AOUT -> PA5 (ADC1_IN5)  模拟信号
 *             - DOUT -> PA6 (GPIO_INPUT)  数字报警（已改为 ppm 阈值判断，PA6 可选）
 *
 * @note      本驱动 ADC 部分直接操作寄存器，不依赖 stm32f1xx_hal_adc.h
 *            仅依赖 stm32f1xx.h（芯片寄存器定义）
 *
 * @fix       2026-06-14: 卡死问题修复
 *             - ADC 位操作改为直接数字掩码，不依赖 stm32f1xx.h 宏名
 *             - ADC_ReadOnce 加超时保护，避免永久死循环
 *             - 校准流程加超时保护
 */

#include "mq2.h"
#include <math.h>

/* ======================== 私有宏（直接写寄存器位值，不依赖头文件宏） ======================== */
#define MQ2_ADC_CHANNEL    5           /* PA5 = ADC1_IN5 */

/* ADC1 寄存器位掩码（STM32F1 参考手册，直接写值，避免宏名不匹配） */
#define MQ2_ADC_SR_EOC         (0x00000002UL)  /* SR  bit1: 转换完成标志 */
#define MQ2_ADC_CR2_ADON       (0x00000001UL)  /* CR2 bit0: ADC 使能 */
#define MQ2_ADC_CR2_CONT       (0x00000002UL)  /* CR2 bit1: 连续转换模式 */
#define MQ2_ADC_CR2_CAL        (0x00000004UL)  /* CR2 bit2: 校准使能 */
#define MQ2_ADC_CR2_RSTCAL    (0x00000008UL)  /* CR2 bit3: 复位校准寄存器 */
#define MQ2_ADC_CR2_SWSTART   (0x00400000UL)  /* CR2 bit22: 软件触发启动 */

/* RCC 时钟使能位 */
#define MQ2_RCC_APB2_IOPAEN   (0x00000004UL)  /* IOPAEN: GPIOA 时钟 */
#define MQ2_RCC_APB2_ADC1EN   (0x00000200UL)  /* ADC1EN: ADC1 时钟 */

/* ADC 采样次数 */
#define MQ2_ADC_SAMPLE_CNT   10

/* ADC 等待超时（循环次数，约 1ms @72MHz） */
#define MQ2_ADC_TIMEOUT       50000

/* ======================== 内部函数 ======================== */

/**
 * @brief  配置 PA5 为模拟输入（ADC 通道）
 */
static void MQ2_ADC_GPIO_Init(void)
{
    /* 使能 GPIOA 时钟 */
    RCC->APB2ENR |= MQ2_RCC_APB2_IOPAEN;

    /* PA5: 模拟输入模式 (CNF=00, MODE=00) */
    /* 清除 bit[21:20] 和 bit[23:22] → CNF5=00, MODE5=00 */
    GPIOA->CRL &= ~(0xFUL << (5 * 4));
    /* 模拟输入模式：CNF=00, MODE=00，清除后即为00，无需再设 */
}

/**
 * @brief  配置 PA6 为浮空输入（DOUT，可选）
 */
static void MQ2_DOUT_GPIO_Init(void)
{
    /* PA6: 浮空输入 (CNF=01, MODE=00) */
    GPIOA->CRL &= ~(0xFUL << (6 * 4));  /* 清除 */
    GPIOA->CRL |=  (0x4UL << (6 * 4));  /* CNF=01, MODE=00 → 浮空输入 */
}

/**
 * @brief  初始化 ADC1（直接操作寄存器）
 *         - 独立模式，12bit 分辨率，右对齐
 *         - 单次转换模式，软件触发
 *         - 使能内部校准
 */
static int MQ2_ADC_Init_Inner(void)
{
    /* 1. 使能 ADC1 时钟 */
    RCC->APB2ENR |= MQ2_RCC_APB2_ADC1EN;

    /* 2. ADC 预分频：PCLK2=72MHz → ADC_CLK=72/6=12MHz（必须 ≤14MHz）*/
    /* ADCPRE[1:0] in CFGR[15:14] */
    /* 00=2分频, 01=4分频, 10=6分频, 11=8分频 */
    RCC->CFGR &= ~(0x3UL << 14);   /* 清除 ADCPRE */
    RCC->CFGR |=  (0x2UL << 14);   /* 10 = 6分频 → 72/6=12MHz */

    /* 3. ADC 配置寄存器 */
    ADC1->CR1 = 0x00000000;   /* 独立模式，禁用扫描/AWD */
    ADC1->CR2 = 0x00000000;   /* 单次转换，软件触发，数据右对齐 */

    /* 4. 采样时间：ADC1_IN5 (PA5) → SMP5[2:0] in SMPR2[17:15] */
    /* 011 = 55.5 cycles → 12.5+55.5=68 ADC cycles */
    ADC1->SMPR2 &= ~(0x7UL << (3 * 5));   /* 清除 SMP5 */
    ADC1->SMPR2 |=  (0x3UL << (3 * 5));   /* 011 = 55.5 cycles */

    /* 5. 转换序列：规则序列 1 个转换，通道5 */
    ADC1->SQR1 = 0x00000000;   /* L[3:0]=0000 → 1个转换 */
    ADC1->SQR3 = (MQ2_ADC_CHANNEL & 0x1F);  /* 第1个转换 = 通道5 */

    /* 6. 使能 ADC（第一次置位 ADON：给 ADC 上电）*/
    ADC1->CR2 |= MQ2_ADC_CR2_ADON;
    /* 上电稳定延时（t_STAB ≈ 1μs，这里延时约 10μs @72MHz）*/
    for (volatile uint32_t i = 0; i < 1000; i++);

    /* 7. ADC 校准（F1 必须执行）*/
    /* 7.1 复位校准寄存器 */
    ADC1->CR2 |= MQ2_ADC_CR2_RSTCAL;
    /* 等待复位完成（RSTCAL 硬件自动清零）*/
    uint32_t timeout = MQ2_ADC_TIMEOUT;
    while ((ADC1->CR2 & MQ2_ADC_CR2_RSTCAL) && (--timeout > 0));

    /* 7.2 启动校准 */
    ADC1->CR2 |= MQ2_ADC_CR2_CAL;
    /* 等待校准完成（CAL 硬件自动清零）*/
    timeout = MQ2_ADC_TIMEOUT;
    while ((ADC1->CR2 & MQ2_ADC_CR2_CAL) && (--timeout > 0));

    if (timeout == 0)
        return -1;  /* 校准超时 */

    return 0;
}

/**
 * @brief  启动一次 ADC 转换并读取结果（带超时保护）
 * @retval ADC 原始值 (0~4095)，超时返回 0
 */
static uint16_t MQ2_ADC_ReadOnce(void)
{
    /* 1. 清除状态位（写 0 清除 EOC 和 AWD）*/
    ADC1->SR = 0x00000000;

    /* 2. 启动转换（软件触发：置位 SWSTART）*/
    ADC1->CR2 |= MQ2_ADC_CR2_SWSTART;
    /* 同时需要保持 ADON=1（已经在上电后一直保持）*/

    /* 3. 等待转换完成（EOC 位置位）*/
    uint32_t timeout = MQ2_ADC_TIMEOUT;
    while (!(ADC1->SR & MQ2_ADC_SR_EOC) && (--timeout > 0));

    /* 4. 超时或完成，读取数据寄存器 */
    if (timeout == 0)
        return 0;  /* 超时返回 0 */

    return (uint16_t)(ADC1->DR & 0x0FFF);
}

/**
 * @brief  多次采样取平均
 * @retval ADC 原始平均值 (0~4095)
 */
static uint16_t MQ2_ADC_ReadAverage(void)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < MQ2_ADC_SAMPLE_CNT; i++)
    {
        sum += MQ2_ADC_ReadOnce();
    }
    return (uint16_t)(sum / MQ2_ADC_SAMPLE_CNT);
}

/* ======================== 对外初始化接口 ======================== */

int MQ2_Init(void)
{
    MQ2_ADC_GPIO_Init();   /* PA5 模拟输入 */
    MQ2_DOUT_GPIO_Init();  /* PA6 输入（可选）*/
    return MQ2_ADC_Init_Inner();  /* ADC1 初始化 + 校准 */
}

/* ======================== 数据读取 ======================== */

int MQ2_ReadAll(MQ2_Data_t *data)
{
    if (data == NULL)
        return -1;

    /* 1. 读取 ADC 原始值（多次采样取平均）*/
    data->adc_raw = MQ2_ADC_ReadAverage();

    /* 2. 计算模拟电压 (V) */
    data->voltage = data->adc_raw * MQ2_VREF / 4096.0f;

    /* 3. 计算传感器电阻 Rs (kΩ) */
    /*    公式：Rs = RL × (VCC - V) / V */
    if (data->voltage > 0.01f)
    {
        data->rs = MQ2_RL_VALUE * (5.0f - data->voltage) / data->voltage;
    }
    else
    {
        data->rs = 99999.0f;   /* 异常大，表示开路 */
    }

    /* 4. 计算 Rs/R0 比值 */
    if (MQ2_RO_VALUE > 0.0f)
    {
        data->ratio = data->rs / MQ2_RO_VALUE;
    }
    else
    {
        data->ratio = 0.0f;
    }

    /* 5. 估算各种气体浓度 (ppm) */
    data->ppm_methane  = MQ2_RatioToPPM(data->ratio, 0);  /* 甲烷(天然气) */
    data->ppm_propane = MQ2_RatioToPPM(data->ratio, 1);  /* 丙烷(LPG液化石油气) */
    data->ppm_smoke   = MQ2_RatioToPPM(data->ratio, 2);  /* 烟雾 */

    /* 6. 根据 ppm 浓度判断报警（任意气体超标即报警）*/
    data->alarm = MQ2_ReadAlarm(data);

    return 0;
}

/* ======================== 浓度计算 ======================== */

float MQ2_RatioToPPM(float ratio, uint8_t gas)
{
    float a, b;

    if (ratio <= 0.0f)
        return 0.0f;

    switch (gas)
    {
        case 0:  /* 甲烷（天然气 NG） */
            a = 1000.0f;
            b = -2.95f;
            break;
        case 1:  /* 丙烷（LPG 液化石油气） */
            a = 600.0f;
            b = -2.75f;
            break;
        case 2:  /* 烟雾 */
        default:
            a = 650.0f;
            b = -2.80f;
            break;
    }

    /* ppm = A × ratio^B */
    return a * powf(ratio, b);
}

/* ======================== 报警判断 ======================== */

uint8_t MQ2_ReadAlarm(MQ2_Data_t *data)
{
    if (data->ppm_methane > MQ2_ALARM_METHANE)
        return 1;
    if (data->ppm_propane > MQ2_ALARM_PROPANE)
        return 1;
    if (data->ppm_smoke   > MQ2_ALARM_SMOKE)
        return 1;
    return 0;
}

/* ======================== R0 标定 ======================== */

float MQ2_CalibrateR0(void)
{
    uint16_t adc_raw;
    float    voltage, rs;

    adc_raw = MQ2_ADC_ReadAverage();
    voltage = adc_raw * MQ2_VREF / 4096.0f;

    if (voltage < 0.01f)
        return 0.0f;

    rs = MQ2_RL_VALUE * (5.0f - voltage) / voltage;

    /* 洁净空气中，Rs/R0 ≈ 9.9（MQ-2 典型值）*/
    /* → R0 = Rs / 9.9 */
    return rs / 9.9f;
}

/* ======================== 预热 ======================== */

void MQ2_Warmup(MQ2_WarmupCallback callback)
{
    uint8_t total_sec = MQ2_WARMUP_MS / 1000;   /* 30 秒 */

    for (uint8_t i = total_sec; i > 0; i--)
    {
        if (callback != NULL)
        {
            callback(i);
        }
        HAL_Delay(1000);
    }

    if (callback != NULL)
    {
        callback(0);
    }
}
