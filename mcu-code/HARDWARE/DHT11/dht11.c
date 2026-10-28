/**
 * @file    dht11_hal.c
 * @brief   DHT11 温湿度传感器驱动 — HAL 库版（替代原 SPL 版）
 * @note    PB12 接 DHT11 DATA 引脚，需外接 4.7kΩ 上拉到 3.3V
 *          DHT11 通讯时序要求微秒级延时，用 DWT 计数器实现
 */

#include "dht11_hal.h"

/* ===================================================================
 *  微秒级延时 — 基于 DWT (Data Watchpoint and Trace) 周期计数器
 *  STM32F103 @ 72MHz: 1us = 72 个时钟周期
 * =================================================================== */

/**
 * @brief  初始化 DWT 周期计数器（上电后只需调用一次）
 */
static void DWT_Init(void)
{
    /* 若已在 main() 中开启过 DWT→CYCCNT，跳过重复初始化 */
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* 使能 DWT 跟踪 */
        DWT->CYCCNT  = 0;                                 /* 计数器清零  */
        DWT->CTRL   |= DWT_CTRL_CYCCNTENA_Msk;            /* 启动周期计数 */
    }
}

/**
 * @brief  微秒级延时
 * @param  us: 延时的微秒数
 * @note   基于 DWT 硬件计数器，精度约 ±1us（72MHz 下）
 */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;                    /* 记录起始计数值 */
    uint32_t ticks = us * (SystemCoreClock / 1000000U); /* us → 时钟周期 */
    while ((DWT->CYCCNT - start) < ticks);              /* 忙等到目标周期 */
}

/* ===================================================================
 *  GPIO 模式切换 — 用 HAL 库替代原 SPL 的 GPIO_Init
 *  数据线需要在推挽输出（主机发命令）和浮空输入（读传感器数据）之间切换
 * =================================================================== */

/**
 * @brief  将 DATA 引脚配置为推挽输出模式（主机→传感器）
 * @note   每次调用都会重新初始化 GPIO，适合双向单总线协议
 */
static void DHT11_GPIO_Init_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};              /* 结构体清零，避免残留值干扰 */

    __HAL_RCC_GPIOB_CLK_ENABLE();                        /* 使能 GPIOB 时钟 */

    GPIO_InitStruct.Pin   = DHT11_PIN;                   /* PB12                            */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;         /* 推挽输出 → SPL: GPIO_Mode_Out_PP */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;                 /* 不上拉/下拉                      */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;        /* 高速（≥50MHz 级别）              */
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);         /* 写入 GPIO 寄存器                 */
}

/**
 * @brief  将 DATA 引脚配置为浮空输入模式（传感器→主机）
 * @note   DHT11 应答信号是开漏输出的，主机端需浮空输入检测电平
 */
static void DHT11_GPIO_Init_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};              /* 结构体清零，避免残留值干扰 */

    __HAL_RCC_GPIOB_CLK_ENABLE();                        /* 使能 GPIOB 时钟 */

    GPIO_InitStruct.Pin   = DHT11_PIN;                   /* PB12                              */
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;             /* 输入模式 → SPL: GPIO_Mode_IN_FLOATING */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;                 /* 浮空输入（不上拉也不下拉）         */
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);         /* 写入 GPIO 寄存器                   */
}

/* ===================================================================
 *  DHT11 单总线时序操作
 *  参照 DHT11 数据手册的通讯时序图
 * =================================================================== */

/**
 * @brief  主机发送起始信号 + 切换到接收模式
 * @note   时序: DATA拉低≥18ms → 拉高→延时20~40us → 切换到输入
 *         然后 DHT11 会应答 80us 低 + 80us 高
 */
static void DHT11_Rst(void)
{
    DHT11_GPIO_Init_OUT();                   /* ① 先切换为推挽输出模式             */
    DHT11_DQ_OUT(GPIO_PIN_RESET);            /* ② 拉低 DATA 线                    */
    HAL_Delay(20);                            /* ③ 保持低 ≥18ms                    */
    DHT11_DQ_OUT(GPIO_PIN_SET);              /* ④ 拉高 DATA 线                    */
    delay_us(30);                             /* ⑤ 主机拉高 20~40us                */
    DHT11_GPIO_Init_IN();                     /* ⑥ 切换到输入模式，准备收传感器应答 */
}

/**
 * @brief  等待 DHT11 应答信号（起始信号之后调用）
 * @retval 0: DHT11 在线应答正常
 * @retval 1: DHT11 不存在或超时
 * @note   正常应答时序：DHT11 先拉低 80us，再拉高 80us
 */
static uint8_t DHT11_Check(void)
{
    uint8_t retry = 0;

    /* ===== 等待 DHT11 拉低（应答低电平 ~80us） ===== */
    DHT11_GPIO_Init_IN();                                /* 确保是输入模式                   */
    while (DHT11_DQ_IN && retry < 100)                   /* 等待 DATA 线被 DHT11 拉低        */
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;                          /* 超时 → DHT11 无应答，返回错误     */

    /* ===== 等待 DHT11 拉高（应答高电平 ~80us） ===== */
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)                  /* 等待 DATA 线被 DHT11 拉高        */
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100) return 1;                          /* 超时 → 应答异常，返回错误         */

    return 0;                                            /* 应答正常                          */
}

/**
 * @brief  从 DHT11 读取一个数据位（bit）
 * @retval 1 或 0
 * @note   每位数据由一段低电平（~50us）+ 一段高电平表示：
 *         高电平 < 28us → bit=0；高电平 > 28us → bit=1
 */
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0;

    /* === 等待当前位的起始低电平结束 === */
    while (DHT11_DQ_IN && retry < 100)                   /* 等待变为低电平 */
    {
        retry++;
        delay_us(1);
    }
    /* === 等待低电平结束、进入高电平阶段 === */
    retry = 0;
    while (!DHT11_DQ_IN && retry < 100)                  /* 等待变高电平 */
    {
        retry++;
        delay_us(1);
    }

    /* === 延时 40us 后采样 DATA 线：高→bit=1，低→bit=0 === */
    delay_us(40);                                        /* 40us 后高电平已结束（bit=0）或还在（bit=1） */
    if (DHT11_DQ_IN)
        return 1;                                        /* 40us 后仍高 → bit = 1              */
    else
        return 0;                                        /* 40us 后已低 → bit = 0              */
}

/**
 * @brief  从 DHT11 读取一个字节（8 位，高位先出 MSB first）
 * @retval 读取到的字节值
 */
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i, dat;
    dat = 0;
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;                                       /* 左移一位，腾出最低位               */
        dat |= DHT11_Read_Bit();                         /* 读一位填入最低位                   */
    }
    return dat;
}

/* ===================================================================
 *  公开 API — 读取一次温湿度数据
 * =================================================================== */

/**
 * @brief  从 DHT11 读取一次温湿度
 * @param  temp: [out] 温度值 0~50°C（整数部分）
 * @param  humi: [out] 湿度值 20~90%RH（整数部分）
 * @retval 0: 读取成功，校验通过
 * @retval 1: DHT11 无应答或校验失败
 * @note   内部自动处理起始信号→应答→40bit读数→校验
 *         调用间隔建议 ≥100ms，否则 DHT11 可能返回上一次数据
 */
uint8_t DHT11_Read_Data(uint8_t *wendu, uint8_t *shidu)
{
    uint8_t buf[5];                                      /* 5字节缓冲: RH整数,RH小数,T整数,T小数,校验 */
    uint8_t i;

    DWT_Init();                                          /* 确保 DWT 计数器已启动（幂等调用） */

    DHT11_Rst();                                         /* ① 主机发起始信号 */

    if (DHT11_Check() == 0)                              /* ② 等待传感器应答 */
    {
        for (i = 0; i < 5; i++)                          /* ③ 连续读取 5 字节 (40bit) */
        {
            buf[i] = DHT11_Read_Byte();
        }

        /* ④ 校验: 前4字节和应等于第5字节 */
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4])
        {
            *shidu = buf[0];                              /* 湿度整数部分 (DHT11 → buf[1]恒为0) */
            *wendu = buf[2];                              /* 温度整数部分 (DHT11 → buf[3]恒为0) */
        }
        else
        {
            return 1;                                    /* 校验失败 */
        }
    }
    else
    {
        return 1;                                        /* DHT11 无应答 */
    }

    return 0;                                            /* 成功 */
}
