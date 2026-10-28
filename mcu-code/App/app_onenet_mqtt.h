#ifndef APP_ONENET_MQTT_H
#define APP_ONENET_MQTT_H

#include "stdint.h"

typedef struct
{
    char identifier[32]; // 属性标识符
    uint8_t type;        // 数据类型,0:bool    1:u32      2:s32      3:f32      4:str
    union
    {
        uint8_t bool_val; // 布尔值0
        uint32_t u32_val; // 无符号整型值1
        int32_t s32_val;  // 有符号整型值2
        float f32_val;    // 浮点型值3
        char str_val[32]; // 字符串值4
    } value;
} onenet_pub_data_t;

typedef struct
{
    char topic[128];     // 主题
    char json_data[256]; // 数据
} onenet_mqtt_topic_data_t;

typedef struct
{
    char msg_id[32];        // 消息id
    onenet_pub_data_t data; // 数据
} onenet_mqtt_propertySet_t;

#define ONENET_DATA_NUM 10   //设备数量

extern onenet_pub_data_t onenet_data[ONENET_DATA_NUM];

/**
 * @breif   MQTT发布数据
 * @param   data:数据指针
 *          data_num:数据个数
 * @retval  0:成功 1:失败
 */
uint8_t onenet_mqtt_publish_post_data(onenet_pub_data_t *data, uint8_t data_num);

/**
 * @breif   MQTT解析主题数据
 * @param   data:解析数据
 * @retval  0:成功 1:解析失败
 */
uint8_t onenet_mqtt_parse_topic_data(char *data, onenet_mqtt_topic_data_t *data_out);

/**
 * @breif   MQTT获取指定主题数据并解析
 * @param   topic:主题
 *          str:数据
 *          data_out:解析结果
 * @retval  0:成功 1:解析失败 2:未找到主题
 */
uint8_t onenet_mqtt_get_topic_data(char *topic, char *str, onenet_mqtt_topic_data_t *data_out);

/**
 * @breif   MQTT响应主题数据
 * @param   id:消息id
 *          code:响应码
 *          msg:响应信息
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_response_topic(char *id, uint8_t code, char *msg);

/**
 * @breif   获取到MQTT数据，用户处理函数
 * @param   data :接收并解析后的数据
 * @retval  0:成功 1:解析失败
 */
uint8_t onenet_mqtt_user_handle(onenet_pub_data_t *data);

/**
 * @breif   MQTT解析属性设置主题数据
 * @param   json_data:解析数据
 *          data_out:解析结果
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_parse_propertySet_topic(char *json_data, onenet_mqtt_propertySet_t *data_out);

/**
 * @breif   打印主题数据
 * @param   data:主题数据
 * @retval  无
 */
void onenet_mqtt_topic_log(onenet_mqtt_topic_data_t *data);

#endif
