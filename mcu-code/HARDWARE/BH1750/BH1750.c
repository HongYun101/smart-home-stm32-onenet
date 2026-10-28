/**
  * BH1750光照传感器驱动程序（增强版）
  * 使用独立的软件I2C总线（PB6/PB7），不与OLED共用
  *
  * 增强功能：
  *   - I2C 总线卡死恢复（9个SCL脉冲解锁）
  *   - NACK 错误检测（传感器无响应时返回 -1）
  *   - 自动软复位（连续读取失败时重新初始化传感器）
  *   - 数据合理性校验（过滤 0xFFFF 等无效值）
  *
  * 引脚分配：
  * - SCL: PB6
  * - SDA: PB7
  */

#include "main.h"
#include "BH1750.h"
#include <stdint.h>

/* ======================== BH1750 I2C地址 ======================== */
#define BH1750_ADDR             0x23    /* 7位地址，ADDR=Low */
#define BH1750_ADDR_W           (BH1750_ADDR << 1)       /* 写地址：0x46 */
#define BH1750_ADDR_R           ((BH1750_ADDR << 1) | 1)  /* 读地址：0x47 */

/* ======================== BH1750测量模式 ======================== */
#define BH1750_CONT_H_MODE      0x10    /* 连续高分辨率（1 lx，~120ms）*/
#define BH1750_ONE_H_MODE       0x20    /* 单次高分辨率（1 lx，~120ms）*/

/* ======================== 连续失败计数 ======================== */
static uint8_t bh1750_fail_count = 0;   /* 连续读取失败次数 */
#define BH1750_FAIL_THRESHOLD   3       /* 连续失败3次后执行软复位 */

/* ======================== I2C底层函数 ======================== */

static void BH1750_W_SCL(uint8_t BitValue)
{
    HAL_GPIO_WritePin(BH1750_SCL_Port, BH1750_SCL_Pin, (GPIO_PinState)BitValue);
    __NOP(); __NOP(); __NOP(); __NOP();
}

static void BH1750_W_SDA(uint8_t BitValue)
{
    HAL_GPIO_WritePin(BH1750_SDA_Port, BH1750_SDA_Pin, (GPIO_PinState)BitValue);
    __NOP(); __NOP(); __NOP(); __NOP();
}

static uint8_t BH1750_R_SDA(void)
{
    return HAL_GPIO_ReadPin(BH1750_SDA_Port, BH1750_SDA_Pin);
}

/**
  * @brief  I2C 总线恢复
  *         当 SDA 被从机拉低（总线卡死）时，发送 9 个 SCL 脉冲释放总线
  *         这是 I2C 协议标准的总线恢复方法
  */
static void BH1750_I2C_BusRecovery(void)
{
    uint8_t i;

    /* 先确保 SCL 和 SDA 释放 */
    BH1750_W_SCL(1);
    BH1750_W_SDA(1);

    /* 检测 SDA 是否被卡低 */
    if (BH1750_R_SDA() == 0)
    {
        /* SDA 被从机拉低，发送 9 个 SCL 脉冲让从机释放 */
        for (i = 0; i < 9; i++)
        {
            BH1750_W_SCL(0);
            __NOP(); __NOP(); __NOP(); __NOP();
            BH1750_W_SCL(1);
            __NOP(); __NOP(); __NOP(); __NOP();

            /* 如果 SDA 已经恢复高电平，可以提前退出 */
            if (BH1750_R_SDA() != 0)
                break;
        }
    }

    /* 发送 STOP 信号，确保总线回到空闲状态 */
    BH1750_W_SDA(0);
    __NOP(); __NOP();
    BH1750_W_SCL(1);
    __NOP(); __NOP();
    BH1750_W_SDA(1);
    __NOP(); __NOP();
}

static void BH1750_I2C_Start(void)
{
    BH1750_W_SDA(1);
    BH1750_W_SCL(1);
    BH1750_W_SDA(0);
    BH1750_W_SCL(0);
}

static void BH1750_I2C_Stop(void)
{
    BH1750_W_SDA(0);
    BH1750_W_SCL(1);
    BH1750_W_SDA(1);
}

/**
  * @brief  I2C 发送一个字节（带 ACK 检测）
  * @retval 0=ACK(从机应答), 1=NACK(从机无应答)
  */
static uint8_t BH1750_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++)
    {
        BH1750_W_SDA(!!(Byte & (0x80 >> i)));
        BH1750_W_SCL(1);
        BH1750_W_SCL(0);
    }

    BH1750_W_SDA(1);
    BH1750_W_SCL(1);
    ack = BH1750_R_SDA();
    BH1750_W_SCL(0);

    return ack;
}

static uint8_t BH1750_I2C_ReadByte(uint8_t Ack)
{
    uint8_t i;
    uint8_t Byte = 0;

    BH1750_W_SDA(1);

    for (i = 0; i < 8; i++)
    {
        BH1750_W_SCL(1);
        Byte = (Byte << 1) | BH1750_R_SDA();
        BH1750_W_SCL(0);
    }

    BH1750_W_SDA(Ack ? 1 : 0);
    BH1750_W_SCL(1);
    BH1750_W_SCL(0);
    BH1750_W_SDA(1);

    return Byte;
}

/* ======================== GPIO 初始化 ======================== */

static void BH1750_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = BH1750_SCL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BH1750_SCL_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BH1750_SDA_Pin;
    HAL_GPIO_Init(BH1750_SDA_Port, &GPIO_InitStruct);

    BH1750_W_SCL(1);
    BH1750_W_SDA(1);
}

/* ======================== 功能函数 ======================== */

/**
  * @brief  BH1750 软复位（发送 Reset 命令 0x07）
  *         不需要重新初始化 GPIO，只发送 I2C 复位命令
  */
static void BH1750_SoftReset(void)
{
    /* 先做总线恢复 */
    BH1750_I2C_BusRecovery();

    /* 发送 Power Down (0x00) */
    BH1750_I2C_Start();
    if (BH1750_I2C_SendByte(BH1750_ADDR_W) == 0)
    {
        BH1750_I2C_SendByte(0x00);  /* Power Down */
    }
    BH1750_I2C_Stop();
    HAL_Delay(5);

    /* 发送 Reset (0x07) */
    BH1750_I2C_Start();
    if (BH1750_I2C_SendByte(BH1750_ADDR_W) == 0)
    {
        BH1750_I2C_SendByte(0x07);  /* Reset */
    }
    BH1750_I2C_Stop();
    HAL_Delay(5);

    /* 发送 Power On (0x01) */
    BH1750_I2C_Start();
    if (BH1750_I2C_SendByte(BH1750_ADDR_W) == 0)
    {
        BH1750_I2C_SendByte(0x01);  /* Power On */
    }
    BH1750_I2C_Stop();
    HAL_Delay(10);
}

void BH1750_Init(void)
{
    BH1750_GPIO_Init();

    /* 总线恢复，防止上电时 I2C 卡死 */
    BH1750_I2C_BusRecovery();

    /* 发送 Power On 命令 */
    BH1750_I2C_Start();
    BH1750_I2C_SendByte(BH1750_ADDR_W);
    BH1750_I2C_SendByte(0x01);
    BH1750_I2C_Stop();

    bh1750_fail_count = 0;
}

/**
  * @brief  BH1750 读取光照强度（增强版）
  * @retval 光照强度(lux)，>=0 为有效值；返回 -1.0 表示读取失败
  *
  * 增强功能：
  *   1. 检测 I2C NACK（传感器无响应）
  *   2. 检测 0xFFFF 无效数据
  *   3. 连续失败自动软复位
  *   4. 总线卡死自动恢复
  */
float BH1750_ReadLight(void)
{
    uint8_t buf[2];
    uint16_t raw;
    float lux;
    uint8_t ack;

    /* ===== 第1步：发送单次高分辨率测量命令 ===== */
    BH1750_I2C_Start();
    ack = BH1750_I2C_SendByte(BH1750_ADDR_W);  /* 发送写地址，检测 ACK */

    if (ack != 0)  /* NACK！传感器无响应 */
    {
        BH1750_I2C_Stop();
        bh1750_fail_count++;

        /* 连续失败超过阈值，执行软复位 */
        if (bh1750_fail_count >= BH1750_FAIL_THRESHOLD)
        {
            BH1750_SoftReset();
            bh1750_fail_count = 0;
        }

        return -1.0f;  /* 返回 -1 表示读取失败 */
    }

    BH1750_I2C_SendByte(BH1750_ONE_H_MODE);  /* 单次高分辨率模式 */
    BH1750_I2C_Stop();

    /* ===== 第2步：等待测量完成 ===== */
    HAL_Delay(120);

    /* ===== 第3步：读取2字节数据 ===== */
    BH1750_I2C_Start();
    ack = BH1750_I2C_SendByte(BH1750_ADDR_R);  /* 发送读地址，检测 ACK */

    if (ack != 0)  /* NACK！传感器无响应 */
    {
        BH1750_I2C_Stop();
        bh1750_fail_count++;

        if (bh1750_fail_count >= BH1750_FAIL_THRESHOLD)
        {
            BH1750_SoftReset();
            bh1750_fail_count = 0;
        }

        return -1.0f;
    }

    buf[0] = BH1750_I2C_ReadByte(0);  /* 高字节，ACK */
    buf[1] = BH1750_I2C_ReadByte(1);  /* 低字节，NACK */
    BH1750_I2C_Stop();

    /* ===== 第4步：数据校验 ===== */
    raw = ((uint16_t)buf[0] << 8) | buf[1];

    /* 0xFFFF 表示 I2C 读取失败（SDA 持续高电平）*/
    if (raw == 0xFFFF)
    {
        bh1750_fail_count++;

        if (bh1750_fail_count >= BH1750_FAIL_THRESHOLD)
        {
            BH1750_SoftReset();
            bh1750_fail_count = 0;
        }

        return -1.0f;
    }

    /* 0x0000 也可能是无效数据（传感器未开始测量）*/
    if (raw == 0x0000)
    {
        return 0.0f;  /* 返回 0 而不是错误，因为黑暗环境确实可能是 0 */
    }

    /* ===== 第5步：计算光照强度 ===== */
    lux = (float)raw / 1.2f;

    /* 合理性检查：BH1750 量程 1~65535 lux，超过 100000 不合理 */
    if (lux > 100000.0f)
    {
        bh1750_fail_count++;
        return -1.0f;
    }

    /* 读取成功，清零失败计数 */
    bh1750_fail_count = 0;

    return lux;
}
