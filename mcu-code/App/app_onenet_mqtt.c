#include "app_main.h"
#include "app_onenet_mqtt.h"

#include "esp8266.h"

#include "stdio.h"
#include "string.h"
#include "math.h"

// clang-format off
onenet_pub_data_t onenet_data[ONENET_DATA_NUM] = {
    [0] = {.identifier = "wendu",               .value.f32_val = 0.0f,   .type = 3},
    [1] = {.identifier = "shidu",               .value.f32_val = 0.0f,   .type = 3},
    [2] = {.identifier = "LED1",                .value.bool_val = 0,     .type = 0},
	[3] = {.identifier = "JDQ",                 .value.bool_val = 0,     .type = 0},
	[4] = {.identifier = "LightLux",            .value.f32_val = 0.0f,   .type = 3},
	[5] = {.identifier = "jiawan",              .value.f32_val = 0.0f,   .type = 3},
	[6] = {.identifier = "bingwan",             .value.f32_val = 0.0f,   .type = 3},
	[7] = {.identifier = "yanwu",               .value.f32_val = 0.0f,   .type = 3},
	[8] = {.identifier = "weixian",             .value.bool_val = 0,     .type = 2},
	[9] = {.identifier = "PWM",                 .value.s32_val = 0,      .type = 1},
	
	
    //[3] = {.identifier = "JDQ",                 .value.u32_val = 0,     .type = 1},
    //[4] = {.identifier = "JDQ",               	.value.bool_val = 0,    .type = 0},
};
// clang-format on

// 用于存储MQTT发布的json数据，如果onenet的属性数量多，需要调大缓冲区，并调节 AT_SEND_BUFFER_SIZE 宏大小
static char onenet_mqtt_post_json_buf[256];

/**
 * @breif   MQTT发布数据
 * @param   无
 * @retval  0:成功 其他:失败
 */
static uint8_t onenet_mqtt_publish(char *topic, void *payload, uint16_t payload_len)
{
    return esp8266_mqtt_publish(topic, payload, payload_len + 2, 0, 0); //+2计算换行回车字符
}

/**
 * @breif   MQTT发布数据
 * @param   data:数据指针
 *          data_num:数据个数
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_publish_post_data(onenet_pub_data_t *data, uint8_t data_num)
{
    memset(onenet_mqtt_post_json_buf, 0, sizeof(onenet_mqtt_post_json_buf));

    // 构建JSON头部
    int len = snprintf(onenet_mqtt_post_json_buf, sizeof(onenet_mqtt_post_json_buf), "{\"id\":\"123\",\"params\":{");

    if (len >= sizeof(onenet_mqtt_post_json_buf))
        return 0xFF; // 缓冲区溢出检查

    // 循环处理每个数据点
    for (uint8_t i = 0; i < data_num; i++)
    {
        // 添加逗号（除第一个元素外）
        if (i > 0)
        {
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len, ",");
            if (len >= sizeof(onenet_mqtt_post_json_buf))
                return 0xFF;
        }

        // 添加键名
        len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                        "\"%s\":{\"value\":", data[i].identifier);
        if (len >= sizeof(onenet_mqtt_post_json_buf))
            return 0xFF;

        // 根据类型添加值
        switch (data[i].type)
        {
        case 0: // 布尔型
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                            "%s", data[i].value.bool_val == 1 ? "true" : "false");
            break;

        case 1: // 无符号32位整型
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                            "%u", data[i].value.u32_val);
            break;

        case 2: // 有符号32位整型
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                            "%d", data[i].value.s32_val);
            break;

        case 3: // 浮点型
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                            "%.1f", data[i].value.f32_val);
            break;
        case 4: // 字符串型
            len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len,
                            "\"%s\"", data[i].value.str_val);
            break;

        default:
            return 0xFE; // 无效数据类型
        }
        if (len >= sizeof(onenet_mqtt_post_json_buf))
            return 0xFF;

        // 闭合value对象
        len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len, "}");
        if (len >= sizeof(onenet_mqtt_post_json_buf))
            return 0xFF;
    }

    // 构建JSON尾部
    len += snprintf(onenet_mqtt_post_json_buf + len, sizeof(onenet_mqtt_post_json_buf) - len, "}}");
    if (len >= sizeof(onenet_mqtt_post_json_buf))
        return 0xFF;

    // 发送MQTT消息
    return onenet_mqtt_publish((char *)mqtt_info.publish_topic_post,
                               onenet_mqtt_post_json_buf, len);
}

/**
 * @breif   MQTT解析主题数据
 * @param   data:解析数据
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_parse_topic_data(char *data, onenet_mqtt_topic_data_t *data_out)
{
    char *topic_start = NULL;
    char *topic_end = NULL;
    char *json_start = NULL;
    char *json_end = NULL;

    memset(data_out, 0, sizeof(onenet_mqtt_topic_data_t));

    // 查找主题起始位置（$符号）
    topic_start = strchr(data, '$');
    if (!topic_start)
        return 1;

    // 查找主题结束位置（第一个逗号）
    topic_end = strchr(topic_start, ',');
    if (!topic_end)
        return 2;

    // 提取主题（从$到逗号）
    size_t topic_len = topic_end - topic_start;
    if (topic_len >= sizeof(data_out->topic))
        topic_len = sizeof(data_out->topic) - 1;

    strncpy(data_out->topic, topic_start, topic_len);
    data_out->topic[topic_len] = '\0';

    // 查找JSON起始位置（第一个{）
    json_start = strchr(topic_end, '{');
    if (!json_start)
        return 3;

    // 查找JSON结束位置（最后一个}）
    json_end = strrchr(json_start, '}');
    if (!json_end)
        return 4;

    // 提取JSON数据（包含花括号）
    size_t json_len = json_end - json_start + 1;
    if (json_len >= sizeof(data_out->json_data))
        json_len = sizeof(data_out->json_data) - 1;

    strncpy(data_out->json_data, json_start, json_len);
    data_out->json_data[json_len] = '\0';

    return 0;
}

/**
 * @breif   MQTT响应主题数据
 * @param   id:消息id
 *          code:响应码
 *          msg:响应信息
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_response_topic(char *id, uint8_t code, char *msg)
{
    memset(onenet_mqtt_post_json_buf, 0, sizeof(onenet_mqtt_post_json_buf));
    snprintf(onenet_mqtt_post_json_buf, sizeof(onenet_mqtt_post_json_buf),
             "{"
             "\"id\":\"%s\","
             "\"code\":%d,"
             "\"msg\":\"%s\""
             "}",
             id, code, msg);
    return onenet_mqtt_publish((char *)mqtt_info.publish_topic_set,
                               onenet_mqtt_post_json_buf, strlen(onenet_mqtt_post_json_buf));
}

/**
 * @breif   MQTT获取指定主题数据并解析
 * @param   topic:主题
 *          str:数据
 *          data_out:解析结果
 * @retval  0:成功 1:解析失败 2:未找到主题
 */
uint8_t onenet_mqtt_get_topic_data(char *topic, char *str, onenet_mqtt_topic_data_t *data_out)
{
    char *line_start = str;
    char *line_end;
    uint8_t res = 0;

    while (line_start && *line_start) // 接收的一次串口数据，可能包含多个主题的数据，这里以换行符作为分隔符，逐个解析
    {
        res = 2;
        line_end = strchr(line_start, '\n');

        if (line_end) // 临时截断该行
            *line_end = '\0';

        if (strstr(line_start, topic)) // 检查是否是目标主题行
        {
            if (line_end)
                *line_end = '\n'; // 恢复原数据
            // printf("提取行: %s\r\n", line_start);
            res = onenet_mqtt_parse_topic_data(line_start, data_out); // onenet格式解析
            break;
        }

        if (line_end)
            *line_end = '\n'; // 恢复原数据
        line_start = line_end ? (line_end + 1) : NULL;
    }

    return res;
}

/**
 * @breif   打印主题数据
 * @param   data:主题数据
 * @retval  无
 */
void onenet_mqtt_topic_log(onenet_mqtt_topic_data_t *topic_data)
{
    printf("\r\n");
    printf("mqtt get topic data:\r\n");
    printf("topic:%s\r\n", topic_data->topic);
    printf("data:%s\r\n", topic_data->json_data);
    printf("\r\n");
}

/**
 * @breif   MQTT解析属性设置主题数据
 * @param   json_data:解析数据
 *          data_out:解析结果
 * @retval  0:成功 其他:失败
 */
uint8_t onenet_mqtt_parse_propertySet_topic(char *json_data, onenet_mqtt_propertySet_t *data_out)
{
    char *ptr = NULL;

    memset(data_out, 0, sizeof(onenet_mqtt_propertySet_t));

    ptr = strstr(json_data, "\"id\":\"");
    if (!ptr)
        return 1;

    sscanf(ptr + 6, "%31[^\"]", data_out->msg_id); // +6 跳过 "id":"

    ptr = strstr(json_data, "\"params\":{\"");
    if (!ptr)
        return 2;

    // 提取 identifier
    sscanf(ptr + 11, "%31[^\"]", data_out->data.identifier); //  +11 跳过 "params":{"

    // 找到冒号
    char *colon_ptr = strchr(ptr + 11 + strlen(data_out->data.identifier), ':');
    if (colon_ptr)
    {
        char value_str[32];
        sscanf(colon_ptr + 1, "%15[^,} \t\n\r]", value_str);

        // 根据类型解析值
        for (int i = 0; i < sizeof(onenet_data) / sizeof(onenet_pub_data_t); i++)
        {
            if (strcmp(onenet_data[i].identifier, data_out->data.identifier) == 0)
            {
                data_out->data.type = onenet_data[i].type;
                break;
            }
        }

        // 根据类型解析值
        switch (data_out->data.type)
        {
        case 0: // bool
            if (strcmp(value_str, "true") == 0)
                data_out->data.value.u32_val = 1;
            else if (strcmp(value_str, "false") == 0)
                data_out->data.value.u32_val = 0;
            break;
        case 1: // u32
        case 2: // s32
            sscanf(value_str, "%d", &data_out->data.value.u32_val);
            break;
        case 3: // f32
            sscanf(value_str, "%f", &data_out->data.value.f32_val);
            break;
        case 4: // string
            strncpy(data_out->data.value.str_val, value_str, sizeof(data_out->data.value.str_val) - 1);
        }
    }

    return 0; // 成功
}
