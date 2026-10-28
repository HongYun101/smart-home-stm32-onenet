/**
 * @file    at_port.c
 * @version v1.1
 * @date    2026-03-20
 * @author  ZeroOneLab
 * @website https://github.com/ZeroOneLab/EmbATlink.git
 *
 * @license MIT License
 * Copyright (c) 2026 ZeroOneLab
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "at_port.h"
#include "at_driver.h"

#include "main.h"
#include "usart.h"

// clang-format off
/* AT指令表 */
const at_cmd_config_t at_cmd_table[AT_CMD_LAST] = {
    [AT_CMD_DEFAULT]        =   {"AT_CMD_DEFAULT",      NULL,   0,  0,  0}, // 默认指令
    [ESP_AT]                =   {"AT",                  "OK",   50, 20, 200},
    [ESP_RST]               =   {"AT+RST",              "OK",   3,  20, 8000},
    [ESP_RESTORE]           =   {"AT+RESTORE",          "OK",   3,  20, 8000},
    [ESP_ATE]               =   {"ATE",                 "OK",   3,  20, 200},
    [ESP_WIFI_SET_MODE]     =   {"AT+CWMODE=",          "OK",   3,  20, 200},
    [ESP_WIFI_CONNECT]      =   {"AT+CWJAP=",           "OK",   3,  20, 8000},
    [ESP_MQTT_SET_INFO]     =   {"AT+MQTTUSERCFG=",     "OK",   3,  20, 200},
    [ESP_MQTT_CONNECT]      =   {"AT+MQTTCONN=",        "OK",   3,  20, 2000},
    [ESP_MQTT_SUBSCRIBE]    =   {"AT+MQTTSUB=",         "OK",   3,  20, 200},
    [ESP_MQTT_PUBLISH_RAW]  =   {"AT+MQTTPUBRAW=",      NULL,   1,  20, 100},
    [ESP_MQTT_PUBLISH_DATA] =   {"",                    "OK",   1,  20, 200},
};

/* AT监听指令表 */
const char *at_monitor_key_table[AT_MONITOR_LAST] = {
    [AT_MONITOR_DEFAULT]    =   "AT_MONITOR_DEFAULT", // 默认指令
    [ESP_MQTT_RECV_DATA]    =   "+MQTTSUBRECV:",    
};
//clang-format on

/* AT接收数据缓存,目前单次最大接收1字节 */
uint8_t at_rx_data[AT_LUN_MAX][1];

/**
 * @brief 延时函数
 * @param xms: 延时时间
 * @retval 无
 */
void at_port_delay_ms(uint32_t xms)
{
    HAL_Delay(xms);
}

/**
 * @brief 获取系统时间
 * @param 无
 * @retval 系统时间
 */
uint32_t at_port_get_tick_ms(void)
{
    return HAL_GetTick();
}

/**
 * @brief AT串口初始化
 * @param lun: 串口编号
 * @retval 无
 */
void at_port_init(uint8_t lun)
{
    switch (lun)
    {
    case 0:
        HAL_UART_Receive_IT(&huart3, (uint8_t *)at_rx_data[lun], 1);
        break;
    case 1:
        break;

    default:
        break;
    }
}

/**
 * @brief AT串口发送数据
 * @param lun: 串口编号
 * @param buf: 发送数据缓冲区
 * @param len: 发送数据长度
 * @retval 无
 */
void at_port_uart_transmit(uint8_t lun, const char *buf, uint16_t len)
{
    switch (lun)
    {
    case 0:
        HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 1000);
        break;
    case 1:
        break;

    default:
        break;
    }
}

/**
 * @brief 进入临界区
 * @param lun: 串口编号
 * @retval 无
 */
void at_port_enter_critical(uint8_t lun)
{
    switch (lun)
    {
    case 0:
        break;
    case 1:
        break;

    default:
        break;
    }
}

/**
 * @brief 退出临界区
 * @param lun: 串口编号
 * @retval 无
 */
void at_port_exit_critical(uint8_t lun)
{
    switch (lun)
    {
    case 0:
        break;
    case 1:
        break;

    default:
        break;
    }
}
