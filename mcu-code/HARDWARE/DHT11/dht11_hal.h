/**
 * @file    dht11_hal.h
 * @brief   DHT11 温湿度传感器 HAL 库驱动 (PB12)
 * @note    单次通讯最大 3ms，连续采样间隔建议 >= 100ms
 *          STM32F103C8T6 @ 72MHz, SysTick 1ms
 * @ref     原 SPL 版本 → HAL 重构
 */

#ifndef __DHT11_HAL_H
#define __DHT11_HAL_H

#include "stm32f1xx_hal.h"          /* HAL 库主头文件 */

/* ==================== 引脚宏定义 ==================== */
#define DHT11_PORT      GPIOB       /* DHT11 DATA 所在的 GPIO 端口  */
#define DHT11_PIN       GPIO_PIN_12 /* DHT11 DATA → PB12             */

/* 读写 DATA 线的便捷宏（内部用） */
#define DHT11_DQ_OUT(x) HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, (x))  /* 写电平 */
#define DHT11_DQ_IN     HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)        /* 读电平 */

/* ==================== API 函数声明 ==================== */

/**
 * @brief  读取一次温湿度数据
 * @param  temp: [out] 温度值 (0~50°C)
 * @param  humi: [out] 湿度值 (20~90%RH)
 * @retval 0=成功, 1=失败
 */
uint8_t DHT11_Read_Data(uint8_t *wendu, uint8_t *shidu);

#endif /* __DHT11_HAL_H */
