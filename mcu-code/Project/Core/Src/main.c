/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c          // 当前文件名：主函数文件
  * @brief          : Main program body // 文件说明：程序主体入口
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics. // 版权：意法半导体
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

// 系统自动生成的头文件包含：外设初始化相关头文件
/* Includes ------------------------------------------------------------------*/
#include "main.h"    // 工程总头文件，HAL库基础定义、宏、引脚声明
#include "tim.h"     // 定时器(TIM)外设初始化函数声明
#include "usart.h"   // 串口(UART)外设初始化函数声明
#include "gpio.h"    // GPIO引脚初始化函数声明
#include "OLED.h"	//oled 硬件驱动
#include "OLED_Data.h"	//oled字库
#include "dht11_hal.h"	//dht11硬件驱动
#include "BH1750.h"  //bh1750硬件驱动
#include "mq2.h"	//mq-2气体检测
//C库
#include <string.h>
#include <stdio.h>


// 私有头文件区域：用户自定义头文件放在这里
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_main.h"  // 引入用户应用层主函数头文件
/* USER CODE END Includes */

// 私有类型定义区：结构体、枚举、类型别名等
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

// 私有宏定义区：#define 常量、宏
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

// 私有宏函数区
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

// 私有全局变量区
/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

// 私有函数声明
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);  // 系统时钟配置函数声明
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

// 用户私有代码段
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.  // 程序入口函数
  * @retval int                          // 返回值类型
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/  // 单片机初始化部分

  // HAL库底层初始化：复位外设、初始化Flash、系统滴答定时器SysTick
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  // 调用函数，配置单片机系统时钟
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  // 依次初始化所有由STM32CubeMX配置的外设
  MX_GPIO_Init();        // 初始化所有GPIO引脚
  MX_USART1_UART_Init(); // 初始化串口1
  MX_USART3_UART_Init(); // 初始化串口3
  MX_TIM3_Init();       // 初始化定时器3
  BH1750_Init();		//初始化BH1750
  OLED_Init();			//初始化OLED
  MQ2_Init();         //初始化MQ-2（ADC+GPIO全部内部完成）

  
  //======================默认显示=================================
  OLED_ShowString(35, 0, "设备信息", OLED_8X16);
  OLED_ShowString(35, 16, "WIFI连接中", OLED_8X16);
  OLED_ShowString(35,48,"30秒预热...",OLED_8X16);
//  OLED_Clear();
//    OLED_ShowString(35, 0, "设备信息", OLED_8X16);
//	OLED_ShowString(0,  16, "温度：", OLED_8X16);   // 标签只画一次
//	OLED_ShowString(48, 16, "℃",    OLED_8X16);
//	OLED_ShowString(55, 16, "湿度：", OLED_8X16);
//	OLED_ShowString(103, 16, "％",   OLED_8X16);
//	OLED_ShowString(55, 32, "光照：", OLED_8X16);
//	OLED_ShowString(103, 32, "烟雾：", OLED_8X16);
  OLED_Update();		//写入/更新
  esp8266_reset();
  
  /* USER CODE BEGIN 2 */
  app_main();  // 跳转执行用户应用主函数
  /* USER CODE END 2 */
  /* Infinite loop */  // 主循环，程序死循环运行
  /* USER CODE BEGIN WHILE */
  while (1)  // 无限循环
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    // 此处可添加循环内业务代码
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration  // 系统时钟配置函数
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};  // 时钟振荡器配置结构体
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0}; // 系统总线时钟配置结构体

  /** 配置片内/片外振荡器、PLL锁相环 */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; // 选择高速外部晶振(HSE)
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;                  // 开启外部高速晶振
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;    // 外部晶振分频：不分频
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;                   // 开启内部高速晶振(备用)
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;               // 开启PLL锁相环
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;        // PLL时钟源选择外部晶振
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;               // PLL倍频：9倍

  // 写入振荡器配置，配置失败则进入错误处理
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** 配置CPU、AHB、APB1、APB2总线时钟 */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2; // 配置全部时钟总线
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // 系统时钟源选择PLL输出
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;        // AHB总线：不分频
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;         // APB1总线：2分频
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;         // APB2总线：不分频

  // 写入时钟总线配置，Flash等待周期设为2，失败则进入错误处理
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence. // 错误处理函数
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();  // 关闭所有中断
  while (1)         // 死循环卡住，程序停止运行，用于故障提示
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

// 断言错误处理：开启完整断言后生效，参数异常时触发
#ifdef USE_FULL_ASSERT
/**
  * @brief  断言出错时，输出出错文件名和行号
  * @param  file: 出错文件指针
  * @param  line: 出错行号
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  // 可自行添加打印代码，输出错误位置
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */




