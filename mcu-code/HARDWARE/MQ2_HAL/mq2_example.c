/**
 * @file    mq2_example.c
 * @brief   MQ-2 传感器使用示例 — 直接复制到 main.c 中即可
 *
 * ========== 使用说明 ==========
 *
 * 本驱动不依赖 CubeMX，ADC 和 GPIO 初始化均在 MQ2_Init() 内完成。
 * 只需：
 *   1. 将 mq2.h 和 mq2.c 加入 Keil 工程
 *   2. 在 main.c 中包含 #include "mq2.h"
 *   3. 先调用 HAL_Init() + SystemClock_Config()
 *   4. 再调用 MQ2_Init()
 *   5. 预热后循环调用 MQ2_ReadAll()
 *
 * ========== 硬件连接 ==========
 *
 *   MQ-2 模块    ->    STM32F103C8T6
 *   ----------------------------------------
 *   VCC         ->    5V（传感器供电）
 *   GND         ->    GND
 *   AOUT        ->    PA5（ADC1_IN5 模拟输入）
 *   DOUT        ->    PA6（GPIO 输入，数字报警）
 */

/* ========== 将以下代码复制到你的 main.c 中 ========== */

#include "mq2.h"

/* 全局变量 */
MQ2_Data_t mq2_data;   /* MQ-2 传感器数据 */

int main(void)
{
    /* ===== 1. HAL 库初始化（必须最先调用）===== */
    HAL_Init();

    /* ===== 2. 系统时钟配置（根据你的板子配置）===== */
 //   SystemClock_Config();   /* 你的时钟配置函数，在 main.c 中已有 */

    /* ===== 3. 外设初始化（按需保留）===== */
 //   MX_GPIO_Init();        /* 其他 GPIO（LED、蜂鸣器等）*/
 //   MX_USART1_UART_Init(); /* 如果需要串口打印 */
 //   MX_I2C1_Init();        /* 如果需要 OLED 显示 */

    /* ===== 4. MQ-2 初始化（GPIO + ADC + 校准，驱动内部完成）===== */
    MQ2_Init();

    /* ===== 5. 预热等待（MQ-2 首次上电需要 30s 预热）===== */
    /* 期间可以 OLED 显示 "Preheating..." */
    HAL_Delay(MQ2_WARMUP_MS);  /* 30秒预热 */

    /* ===== 6. 可选：标定 R0（仅在洁净空气环境下执行一次）===== */
    /*
    float ro = MQ2_CalibrateR0();
    // 将打印出的 ro 值写入 mq2.h 的 MQ2_RO_VALUE 宏中
    printf("Calibrated R0 = %.2f kOhm\r\n", ro);
    */

    while (1)
    {
        /* ===== 7. 读取 MQ-2 全部数据 ===== */
        MQ2_ReadAll(&mq2_data);

        /* ===== 8. 串口打印（需已实现 printf 重定向）===== */
        /*
        printf("ADC:%d V:%.2fV Rs:%.1fk Ratio:%.2f\r\n",
               mq2_data.adc_raw,
               mq2_data.voltage,
               mq2_data.rs,
               mq2_data.ratio);

        printf("甲烷:%.0fppm 丙烷:%.0fppm 烟雾:%.0fppm\r\n",
               mq2_data.ppm_methane,
               mq2_data.ppm_propane,
               mq2_data.ppm_smoke);

        printf("Alarm:%s\r\n", mq2_data.alarm ? "YES!" : "No");
        */

        /* ===== 9. OLED 显示示例 ===== */
        /*
        OLED_ShowString(0, 0, "MQ-2 Sensor", 16);
        OLED_ShowString(0, 2, "Met:", 16);
        OLED_ShowNum(40, 2, (uint32_t)mq2_data.ppm_methane, 4, 16);
        OLED_ShowString(0, 4, "Pro:", 16);
        OLED_ShowNum(40, 4, (uint32_t)mq2_data.ppm_propane, 4, 16);
        OLED_ShowString(0, 6, "Smo:", 16);
        OLED_ShowNum(40, 6, (uint32_t)mq2_data.ppm_smoke, 4, 16);
        OLED_ShowString(0, 8, "Alarm:", 16);
        if (mq2_data.alarm)
            OLED_ShowString(60, 8, "YES!", 16);
        else
            OLED_ShowString(60, 8, "No  ", 16);
        */

        /* ===== 10. 报警处理 ===== */
        if (mq2_data.alarm)
        {
            /* 蜂鸣器报警 / LED 闪烁 / 上报 OneNET 云平台 */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);   /* 示例：蜂鸣器响 */
        }
        else
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET); /* 蜂鸣器关 */
        }

        /* 采样周期 1s */
        HAL_Delay(1000);
    }
}
