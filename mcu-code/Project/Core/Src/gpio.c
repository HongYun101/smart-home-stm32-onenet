/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c          // 当前文件名：GPIO配置文件
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.  // 文件说明：提供所有使用到的GPIO引脚的配置代码
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics. // 版权声明：意法半导体
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

// 包含头文件，gpio.h中包含了引脚定义和HAL库GPIO相关声明
/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */
// 用户可在此添加自定义代码，CubeMX重新生成不会覆盖
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
// 用户可在此添加自定义代码，CubeMX重新生成不会覆盖
/* USER CODE END 1 */

/** Configure pins as  // 配置引脚为以下模式：
        * Analog       // 模拟模式
        * Input        // 输入模式
        * Output       // 输出模式
        * EVENT_OUT    // 事件输出模式
        * EXTI         // 外部中断模式
*/
void MX_GPIO_Init(void)  // GPIO初始化函数，在main函数中被调用
{
  // 定义GPIO初始化结构体，用于配置引脚模式、速度、上下拉等参数
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  // 开启对应GPIO端口的时钟，STM32外设默认时钟关闭，必须先开启才能使用
  __HAL_RCC_GPIOD_CLK_ENABLE();  // 开启GPIOD端口时钟
  __HAL_RCC_GPIOA_CLK_ENABLE();  // 开启GPIOA端口时钟
  __HAL_RCC_GPIOB_CLK_ENABLE();  // 开启GPIOB端口时钟
  __HAL_RCC_GPIOC_CLK_ENABLE();  // 开启GPIOC端口时钟

  /*Configure GPIO pin Output Level */
  // 初始化继电器引脚为低电平（GPIO_PIN_RESET）
  //HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  // 初始化LED1引脚为高电平（GPIO_PIN_SET）
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : RELAY_Pin */
  // 配置继电器引脚参数
  //GPIO_InitStruct.Pin = RELAY_Pin;                  // 指定要配置的引脚：RELAY_Pin
  //GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;       // 模式：推挽输出（可输出高低电平，驱动能力强）
  //GPIO_InitStruct.Pull = GPIO_NOPULL;               // 上下拉配置：无上下拉电阻
 // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;      // 输出速度：低速（功耗低，抗干扰好）
  //HAL_GPIO_Init(RELAY_GPIO_Port, &GPIO_InitStruct); // 将配置写入硬件寄存器

  /*Configure GPIO pin : LED1_Pin */
  // 配置LED1引脚参数
  GPIO_InitStruct.Pin = LED1_Pin;                   // 指定要配置的引脚：LED1_Pin
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;       // 模式：推挽输出
  GPIO_InitStruct.Pull = GPIO_NOPULL;               // 无上下拉电阻
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;      // 低速输出
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);  // 将配置写入硬件寄存器
  
	/*Configure GPIO pin : PB0 */
  // 先拉高，避免一上电就把 ESP8266 复位了
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

  // 配置 PB0
  GPIO_InitStruct.Pin   = GPIO_PIN_1;          // PB0
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; // 推挽输出：能主动输出高低电平
  GPIO_InitStruct.Pull  = GPIO_NOPULL;         // 无上下拉：电平完全由我们控制
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // 低速：控制信号不需要高速
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);      // 写入硬件寄存器
  
  
  /* ============================================================
 * MX_GPIO_Init() —— GPIO 初始化
 * CubeMX 生成，在 main.c 中 HAL_Init() 之后调用
 * ============================================================ */

}
 void esp8266_reset(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);   // 拉低
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);     // 拉高
    HAL_Delay(3000);   // 等模块启动
}

/* USER CODE BEGIN 2 */
// 用户可在此添加自定义GPIO相关代码，比如自定义引脚操作函数
/* USER CODE END 2 */
