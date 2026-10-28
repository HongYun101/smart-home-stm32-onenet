/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   串口外设初始化配置文件，由 STM32CubeMX 自动生成
  ******************************************************************************
  * @attention
  * 版权所有 (c) 2026 STMicroelectronics，保留所有权利。
  * 本软件遵循根目录 LICENSE 文件中的授权条款，如无 LICENSE 则按原样提供。
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
// 引入串口头文件，包含 UART_HandleTypeDef 结构体定义和函数声明
#include "usart.h"

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

// ============================================================
// 全局串口句柄声明
// HAL 库通过句柄管理每个外设实例，所有串口操作都通过它进行
// ============================================================

// USART1 句柄：通常用于调试串口（连接 PC 上位机 / 串口助手）
UART_HandleTypeDef huart1;
// USART3 句柄：本项目用于连接 ESP8266 WiFi 模块（PB10/PB11）
UART_HandleTypeDef huart3;

/* ============================================================
 * USART1 初始化函数（由 CubeMX 生成，main.c 中调用）
 * 引脚：PA9(TX) / PA10(RX)，波特率 115200
 * 用途：调试输出 / printf 重定向
 * ============================================================ */
void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */

  // 绑定物理外设寄存器地址到句柄
  huart1.Instance = USART1;

  // 波特率：115200 bps（与上位机/串口助手保持一致）
  huart1.Init.BaudRate = 115200;

  // 数据位：8 位（标准配置，一帧传输 8 bit 有效数据）
  huart1.Init.WordLength = UART_WORDLENGTH_8B;

  // 停止位：1 位（标准配置）
  huart1.Init.StopBits = UART_STOPBITS_1;

  // 奇偶校验：无（双方约定好即可，这里不做错误校验）
  huart1.Init.Parity = UART_PARITY_NONE;

  // 工作模式：发送 + 接收 双向模式
  huart1.Init.Mode = UART_MODE_TX_RX;

  // 硬件流控：关闭（不使用 RTS/CTS 引脚，节省 IO）
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;

  // 过采样：16 倍采样（标准模式，采样精度高，适合 ≤115200 的波特率）
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;

  // 调用 HAL 初始化函数，将上述配置写入寄存器
  // 内部会自动调用 HAL_UART_MspInit() 完成时钟和 GPIO 配置
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    // 初始化失败则进入错误处理（通常死循环或点亮错误 LED）
    Error_Handler();
  }

  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */
}

/* ============================================================
 * USART3 初始化函数（由 CubeMX 生成，main.c 中调用）
 * 引脚：PB10(TX) / PB11(RX)，波特率 115200
 * 用途：与 ESP8266 通信，收发 AT 指令
 * 注意：USART3 开启了中断接收（见 MspInit 中的 NVIC 配置）
 * ============================================================ */
void MX_USART3_UART_Init(void)
{
  /* USER CODE BEGIN USART3_Init 0 */
  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */
  /* USER CODE END USART3_Init 1 */

  // 绑定物理外设：USART3
  huart3.Instance = USART3;

  // 波特率：115200（需与 ESP8266 默认 AT 波特率一致）
  huart3.Init.BaudRate = 115200;

  // 数据位：8 位
  huart3.Init.WordLength = UART_WORDLENGTH_8B;

  // 停止位：1 位
  huart3.Init.StopBits = UART_STOPBITS_1;

  // 无奇偶校验
  huart3.Init.Parity = UART_PARITY_NONE;

  // 双向收发模式
  huart3.Init.Mode = UART_MODE_TX_RX;

  // 无硬件流控
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;

  // 16 倍过采样
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;

  // HAL 初始化，内部自动调用 HAL_UART_MspInit() → 完成 NVIC 中断注册
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN USART3_Init 2 */
  /* USER CODE END USART3_Init 2 */
}

/* ============================================================
 * HAL_UART_MspInit —— 底层硬件资源初始化（MSP = MCU Support Package）
 * 由 HAL_UART_Init() 内部自动回调，用户不需要手动调用
 * 负责：① 开启外设时钟  ② 配置复用 GPIO  ③ 配置 NVIC 中断
 * ============================================================ */
void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{
  // 局部 GPIO 初始化结构体，清零初始化防止脏数据
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* ---------- USART1 底层配置 ---------- */
  if(uartHandle->Instance == USART1)
  {
    /* USER CODE BEGIN USART1_MspInit 0 */
    /* USER CODE END USART1_MspInit 0 */

    // 使能 USART1 外设时钟（APB2 总线）
    // 不开时钟则外设寄存器无法工作
    __HAL_RCC_USART1_CLK_ENABLE();

    // 使能 GPIOA 时钟（PA9/PA10 所在端口）
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /**
     * USART1 引脚映射：
     *   PA9  ──→ USART1_TX（发送）
     *   PA10 ──→ USART1_RX（接收）
     */

    // 配置 PA9 为复用推挽输出（TX 引脚，STM32 主动驱动输出）
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;        // 复用推挽：外设直接控制引脚电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // 高速 IO（适配 115200 以上波特率）
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 配置 PA10 为浮空输入（RX 引脚，接收对方数据，不需要内部上下拉）
    GPIO_InitStruct.Pin  = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;             // 浮空，由外部电路（对方 TX）决定电平
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // USART1 不启用中断（轮询/DMA 模式，或由用户代码在 Init 2 中手动添加）

    /* USER CODE BEGIN USART1_MspInit 1 */
    /* USER CODE END USART1_MspInit 1 */
  }

  /* ---------- USART3 底层配置 ---------- */
  else if(uartHandle->Instance == USART3)
  {
    /* USER CODE BEGIN USART3_MspInit 0 */
    /* USER CODE END USART3_MspInit 0 */

    // 使能 USART3 外设时钟（APB1 总线）
    __HAL_RCC_USART3_CLK_ENABLE();

    // 使能 GPIOB 时钟（PB10/PB11 所在端口）
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /**
     * USART3 引脚映射（连接 ESP8266）：
     *   PB10 ──→ USART3_TX（STM32 发 → ESP8266 RX）
     *   PB11 ──→ USART3_RX（ESP8266 TX → STM32 收）
     */

    // 配置 PB10 为复用推挽输出（TX）
    GPIO_InitStruct.Pin   = GPIO_PIN_10;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 配置 PB11 为浮空输入（RX）
    GPIO_InitStruct.Pin  = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ★ 配置 USART3 中断优先级
    // 参数：IRQ 号、抢占优先级 0（最高）、子优先级 0
    // USART3 用于接收 ESP8266 的 AT 响应，需要及时处理，故优先级最高
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);

    // ★ 使能 USART3 中断（在 NVIC 中打开总开关）
    // 之后调用 HAL_UART_Receive_IT() 才能真正触发中断回调
    HAL_NVIC_EnableIRQ(USART3_IRQn);

    /* USER CODE BEGIN USART3_MspInit 1 */
    /* USER CODE END USART3_MspInit 1 */
  }
}

/* ============================================================
 * HAL_UART_MspDeInit —— 底层硬件资源反初始化
 * 由 HAL_UART_DeInit() 内部自动回调
 * 负责：① 关闭外设时钟  ② 释放 GPIO  ③ 关闭 NVIC 中断
 * ============================================================ */
void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{
  /* ---------- USART1 反初始化 ---------- */
  if(uartHandle->Instance == USART1)
  {
    /* USER CODE BEGIN USART1_MspDeInit 0 */
    /* USER CODE END USART1_MspDeInit 0 */

    // 关闭 USART1 外设时钟，停止外设工作，降低功耗
    __HAL_RCC_USART1_CLK_DISABLE();

    // 释放 PA9（TX）和 PA10（RX），恢复为默认 GPIO 状态
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);

    /* USER CODE BEGIN USART1_MspDeInit 1 */
    /* USER CODE END USART1_MspDeInit 1 */
  }

  /* ---------- USART3 反初始化 ---------- */
  else if(uartHandle->Instance == USART3)
  {
    /* USER CODE BEGIN USART3_MspDeInit 0 */
    /* USER CODE END USART3_MspDeInit 0 */

    // 关闭 USART3 外设时钟
    __HAL_RCC_USART3_CLK_DISABLE();

    // 释放 PB10（TX）和 PB11（RX）
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);

    // 在 NVIC 中关闭 USART3 中断，防止无效中断触发
    HAL_NVIC_DisableIRQ(USART3_IRQn);

    /* USER CODE BEGIN USART3_MspDeInit 1 */
    /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

// 引入 AT 指令驱动层头文件（提供 at_uart_recv_handler、at_rx_data 等接口）
#include "at_driver.h"
// 引入 ESP8266 驱动头文件（提供 AT_LUN_ESP8266 逻辑单元号定义）
#include "esp8266.h"

/* ============================================================
 * HAL_UART_RxCpltCallback —— 串口接收完成中断回调
 * 触发时机：HAL_UART_Receive_IT() 指定的字节数全部接收完毕后
 *           由 HAL 库在中断服务函数（stm32f1xx_it.c）中自动调用
 * 本项目配置为每次接收 1 字节，实现逐字节流式接收 ESP8266 响应
 * ============================================================ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  // 判断是否是 USART3 触发的中断（只处理 ESP8266 通道，忽略其他串口）
  if (huart->Instance == USART3)
  {
    /* ★ 核心步骤1：将收到的 1 字节送入 AT 驱动的环形缓冲区
     *   参数1：AT_LUN_ESP8266      —— 逻辑单元号，标识这是 ESP8266 通道
     *   参数2：at_rx_data[LUN]     —— 指向本次接收数据的缓冲区指针
     *   参数3：1                   —— 本次接收的字节数（1 字节）
     *   内部逻辑：将数据写入环形缓冲区，供上层 AT 解析任务读取
     */
    at_uart_recv_handler(AT_LUN_ESP8266, at_rx_data[AT_LUN_ESP8266], 1);

    /* ★ 核心步骤2：重新挂起中断接收，等待下一个字节
     *   注意：HAL_UART_Receive_IT 是"一次性"的！
     *   每次回调后必须重新调用，否则后续数据将被丢弃
     *   这是 STM32 HAL 中断接收的标准用法
     */
    HAL_UART_Receive_IT(&huart3, at_rx_data[AT_LUN_ESP8266], 1);
  }
}

/* USER CODE END 1 */
