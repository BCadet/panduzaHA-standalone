#include "panduza_dio.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(panduza_dio, LOG_LEVEL_DBG);

#include <zephyr/data/json.h>
#include "app_mqtt.h"
extern app_mqtt_t app;

#define PANDUZA_TOPIC_BASE "pza/"CONFIG_PANDUZA_BENCH_NAME"/"CONFIG_PANDUZA_DEVICE_NAME"-dio-controller"

static const struct json_obj_descr dio_direction_json_desc[] = {
    JSON_OBJ_DESCR_PRIM(dio_direction_t, value, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(dio_direction_t, pull, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(dio_direction_t, polling_cycle, JSON_TOK_NUMBER),
};
static const struct json_obj_descr dio_description_json[] = {
    JSON_OBJ_DESCR_OBJECT(dio_t, direction, dio_direction_json_desc)
};

static const struct json_obj_descr dio_state_json_desc[] = {
    JSON_OBJ_DESCR_PRIM(dio_state_t, active, JSON_TOK_TRUE),
    JSON_OBJ_DESCR_PRIM(dio_state_t, active_low, JSON_TOK_TRUE),
    JSON_OBJ_DESCR_PRIM(dio_state_t, polling_cycle, JSON_TOK_NUMBER),
};
static const struct json_obj_descr dio_state_json[] = {
    JSON_OBJ_DESCR_OBJECT(dio_t, state, dio_state_json_desc)
};

static unsigned char json_encoded_buf[1024];

void panduza_dio_publish_direction(dio_t *dio)
{
    gpio_flags_t flags = 0;
	gpio_pin_get_config(dio->dev, dio->pin, &flags);

    dio->direction.value = (flags&GPIO_OUTPUT_INIT_LOGICAL)?"out":"in";
    dio->direction.pull = (flags&GPIO_PULL_UP)?"up":"down";
    dio->direction.polling_cycle = 1;

    int status = json_obj_encode_buf(dio_description_json, ARRAY_SIZE(dio_description_json),
                dio, json_encoded_buf, sizeof(json_encoded_buf));
    LOG_DBG("json encoding status: %d", status);

    static char topic[] = PANDUZA_TOPIC_BASE"/dio_xx/atts/direction";
    topic[strlen(topic)-17] = '0' + dio->pin/10;
    topic[strlen(topic)-16] = '0' + dio->pin%10;
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void panduza_dio_publish_state(dio_t *dio)
{
	gpio_flags_t flags = 0;
	gpio_pin_get_config(dio->dev, dio->pin, &flags);
    // unsigned char json_encoded_buf[256];

    dio->state.active = gpio_pin_get(dio->dev, dio->pin);
    dio->state.active_low = flags&GPIO_OUTPUT_INIT_LOW;
    dio->state.polling_cycle = 1;

    int status = json_obj_encode_buf(dio_state_json, ARRAY_SIZE(dio_state_json),
                dio, json_encoded_buf, sizeof(json_encoded_buf));
    LOG_DBG("json encoding status: %d", status);
    
    static char topic[] = PANDUZA_TOPIC_BASE"/dio_xx/atts/state";
    topic[strlen(topic)-13] = '0' + dio->pin/10;
    topic[strlen(topic)-12] = '0' + dio->pin%10;
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, false);
}

void panduza_dio_publish_info(dio_t *dio)
{
    static char topic[] = PANDUZA_TOPIC_BASE"/dio_xx/atts/info";
    topic[strlen(topic)-12] = '0' + dio->pin/10;
    topic[strlen(topic)-11] = '0' + dio->pin%10;
    publish(&app.client, topic, PANDUZA_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
}