#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "stdint.h"

typedef struct
{
    char ssid[32];     // WiFi名称
    char password[32]; // WiFi密码
} wifi_info_t;

typedef struct
{
    char host[32];                  // MQTT服务器地址
    uint16_t port;                  // MQTT服务器端口
    char client_id[32];             // MQTT客户端ID
    char username[32];              // MQTT用户名
    char password[256];             // MQTT密码
    char publish_topic_post[128];   // MQTT发布主题：属性上报请求主题
    char subscribe_topic_post[128]; // MQTT订阅主题：属性上报响应主题
    char publish_topic_set[128];    // MQTT发布主题：属性设置响应主题
    char subscribe_topic_set[128];  // MQTT订阅主题：属性设置请求主题
} mqtt_info_t;

extern wifi_info_t wifi_info;
extern mqtt_info_t mqtt_info;

/**
 * @breif   主函数
 * @param   无
 * @retval  无
 */
void app_main(void);

#endif
