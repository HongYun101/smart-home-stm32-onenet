#ifndef ESP8266_H
#define ESP8266_H

#include "stdint.h"
#include "stdio.h"

#define AT_LUN_ESP8266 0

#define ESP_LOG(...) printf(__VA_ARGS__)

/**
 * @brief   初始化ESP8266模块
 * @param   is_frist 是否是第一次初始化
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_init(uint8_t first);

/**
 * @brief   连接WIFI
 * @param   ssid:WIFI名称
 * @param   pwd:WIFI密码
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_wifi_connect(char *ssid, char *pwd);

/**
 * @brief   连接MQTT
 * @param   client_id:MQTT客户端ID
 * @param   username:MQTT用户名
 * @param   password:MQTT密码
 * @param   host:MQTT服务器地址
 * @param   port:MQTT服务器端口
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_connect(char *client_id, char *username, char *password, char *host, uint16_t port);

/**
 * @brief   订阅MQTT
 * @param   topic:MQTT主题
 * @param   qos:QOS等级
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_subscribe(char *topic, uint8_t qos);

/**
 * @brief   发布MQTT
 * @param   topic:MQTT主题
 * @param   data:数据
 * @param   len:数据长度
 * @param   qos:QOS等级
 * @param   retain:是否保留
 * @retval  0:成功，其他:失败
 */
uint8_t esp8266_mqtt_publish(char *topic, char *data, uint16_t len, uint8_t qos, uint8_t retain);

#endif
