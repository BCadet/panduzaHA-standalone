#include "panduza_dio.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(panduza_dio, LOG_LEVEL_DBG);

#include <zephyr/data/json.h>
#include "app_mqtt.h"

#define PZA_DIO_NODE DT_NODELABEL(pza_dio) // Panduza DIO device tree node
// macro to instanciate a DIO
#define PZA_DIO_DECLARE(node_id, prop, idx)                                 \
    {                                                                       \
        .bank = DT_NODE_CHILD_IDX(DT_GPIO_CTLR_BY_IDX(node_id, prop, idx)), \
        .gpio = GPIO_DT_SPEC_GET_BY_IDX(node_id, prop, idx)                 \
    }
// Panduza DIO instances array
dio_t pzaDIO[] = {DT_FOREACH_PROP_ELEM_SEP(PZA_DIO_NODE, pza_gpios, PZA_DIO_DECLARE, (, ))};
static char cmds_strs[ARRAY_SIZE(pzaDIO)][128];      // array of strings to store the path to each dio CMDS
static struct mqtt_topic topics[ARRAY_SIZE(pzaDIO)]; // topic array to subscribe to DIOs CMDS
// MQTT subscription list struct
static const struct mqtt_subscription_list sub_list = {
    .list = topics,
    .list_count = ARRAY_SIZE(topics),
    .message_id = 1u,
};
// gpio callback struct
static struct gpio_callback dio_cb_data;

extern app_mqtt_t app;

#define PANDUZA_TOPIC_BASE "pza/" CONFIG_PANDUZA_BENCH_NAME "/" CONFIG_PANDUZA_DEVICE_NAME "-dio-controller"

static const struct json_obj_descr dio_direction_json_desc[] = {
    JSON_OBJ_DESCR_PRIM(dio_direction_t, value, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(dio_direction_t, pull, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(dio_direction_t, polling_cycle, JSON_TOK_NUMBER),
};
static const struct json_obj_descr dio_direction_json[] = {
    JSON_OBJ_DESCR_OBJECT(dio_t, direction, dio_direction_json_desc)};

static const struct json_obj_descr dio_state_json_desc[] = {
    JSON_OBJ_DESCR_PRIM(dio_state_t, active, JSON_TOK_TRUE),
    JSON_OBJ_DESCR_PRIM(dio_state_t, active_low, JSON_TOK_TRUE),
    JSON_OBJ_DESCR_PRIM(dio_state_t, polling_cycle, JSON_TOK_NUMBER),
};
static const struct json_obj_descr dio_state_json[] = {
    JSON_OBJ_DESCR_OBJECT(dio_t, state, dio_state_json_desc)};

static unsigned char json_encoded_buf[1024];
static char topic[64];

static inline void insert_dio_id(char *str, dio_t *dio, uint8_t offset)
{
    str[offset] = 'A' + dio->bank;
    str[offset + 1] = '0' + dio->gpio.pin / 10;
    str[offset + 2] = '0' + dio->gpio.pin % 10;
}

void panduza_dio_publish_direction(dio_t *dio)
{
    gpio_flags_t flags = 0;
    gpio_pin_get_config_dt(&dio->gpio, &flags);

    LOG_DBG("dio_%c%d flags=%x", dio->bank+'A', dio->gpio.pin, flags);

    dio->direction.value = (flags & GPIO_INPUT) ? "in" : "out";
    dio->direction.pull = (flags & GPIO_PULL_UP) ? "up" : "down";
    dio->direction.polling_cycle = 1;

    int status = json_obj_encode_buf(dio_direction_json, ARRAY_SIZE(dio_direction_json),
                                     dio, json_encoded_buf, sizeof(json_encoded_buf));
    LOG_DBG("json encoding status: %d", status);

    strcpy(topic, PANDUZA_TOPIC_BASE "/dio_xxx/atts/direction");
    insert_dio_id(topic, dio, strlen(topic) - 18);
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void panduza_dio_publish_state(dio_t *dio)
{
    gpio_flags_t flags = 0;
    gpio_pin_get_config_dt(&dio->gpio, &flags);

    dio->state.active = gpio_pin_get_dt(&dio->gpio);
    dio->state.active_low = flags & GPIO_OUTPUT_INIT_LOW;
    dio->state.polling_cycle = 1;

    int status = json_obj_encode_buf(dio_state_json, ARRAY_SIZE(dio_state_json),
                                     dio, json_encoded_buf, sizeof(json_encoded_buf));
    LOG_DBG("json encoding status: %d", status);

    strcpy(topic, PANDUZA_TOPIC_BASE "/dio_xxx/atts/state");
    insert_dio_id(topic, dio, strlen(topic) - 14);
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, false);
}

void panduza_dio_publish_info(dio_t *dio)
{
    strcpy(topic, PANDUZA_TOPIC_BASE "/dio_xxx/atts/info");
    insert_dio_id(topic, dio, strlen(topic) - 13);
    publish(&app.client, topic, PANDUZA_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void dio_interrupt(const struct device *dev, struct gpio_callback *cb,
                   uint32_t pins)
{
    for (int i = 0; i < 32; i++)
    {
        if (pins & BIT(i))
        {
            gpio_flags_t flags = 0;
            gpio_pin_get_config(dev, i, &flags);
            for (uint8_t j = 0; j < ARRAY_SIZE(pzaDIO); j++)
            {
                if (pzaDIO[j].gpio.pin == i && pzaDIO[j].gpio.port == dev)
                {
                    panduza_dio_publish_state(&pzaDIO[j]);
                }
            }

            return;
        }
    }
}

void pza_dio_init()
{
    LOG_INF("ARRAY_SIZE(pzaDIO)=%d", ARRAY_SIZE(pzaDIO));

    for (uint8_t i = 0; i < ARRAY_SIZE(pzaDIO); i++)
    {
        if (!gpio_is_ready_dt(&pzaDIO[i]))
        {
            LOG_ERR("Error: pzaDIO[%d] device %s is not ready\n",
                    i, pzaDIO[i].gpio.port->name);
            return;
        }

        if (gpio_pin_configure_dt(&pzaDIO[i], GPIO_INPUT) != 0)
        {
            LOG_ERR("Error: failed to configure %s pin %d\n", pzaDIO[i].gpio.port->name, pzaDIO[i].gpio.pin);
            return;
        }

        if (gpio_pin_interrupt_configure_dt(&pzaDIO[i], GPIO_INT_EDGE_BOTH) != 0)
        {
            LOG_ERR("Error %d: failed to configure interrupt on %s pin %d\n", pzaDIO[i].gpio.port->name, pzaDIO[i].gpio.pin);
            return;
        }

        gpio_init_callback(&dio_cb_data, dio_interrupt, BIT(pzaDIO[i].gpio.pin));
        gpio_add_callback(pzaDIO[i].gpio.port, &dio_cb_data);

        strncpy(cmds_strs[i], PANDUZA_TOPIC_BASE "/dio_xxx/cmds/#", sizeof(PANDUZA_TOPIC_BASE "/dio_xxx/cmds/#"));
        insert_dio_id(cmds_strs[i], &pzaDIO[i], strlen(cmds_strs[i]) - 10);

        LOG_DBG("adding sub to %s", cmds_strs[i]);

        topics[i].topic.utf8 = cmds_strs[i];
        topics[i].topic.size = strlen(cmds_strs[i]);
        topics[i].qos = MQTT_QOS_0_AT_MOST_ONCE;
    }
}

void pza_dio_publish_all()
{
    for (uint8_t j = 0; j < ARRAY_SIZE(pzaDIO); j++)
    {
        panduza_dio_publish_info(&pzaDIO[j]);
        panduza_dio_publish_state(&pzaDIO[j]);
        panduza_dio_publish_direction(&pzaDIO[j]);
    }
}

void pza_dio_connect()
{
    int ret = mqtt_subscribe(&app.client, &sub_list);
    if (ret != 0)
    {
        LOG_ERR("Failed to subscribe to topics: %d", ret);
    }
}

void pza_dio_handle_event(char *topic, char *payload, uint32_t payload_length)
{
    //detect dio
    char *dio_identifier = topic + sizeof(PANDUZA_TOPIC_BASE "/dio_") - 1;
    char bank = *dio_identifier - 'A';
    int pin = (*(dio_identifier+1) - '0')*10 + (*(dio_identifier+2) - '0');
    LOG_DBG("parsed topic: bank=%c pin=%d", bank+'A', pin);
    dio_t *dio = NULL;
    for (uint8_t j = 0; j < ARRAY_SIZE(pzaDIO); j++)
    {
        if (pzaDIO[j].gpio.pin == pin && pzaDIO[j].bank == bank)
        {
            dio = &pzaDIO[j];
        }
    }
    if(dio == NULL)
    {
        LOG_ERR("dio not found");
        return;
    }
    char *sub_topic = topic + sizeof(PANDUZA_TOPIC_BASE "/dio_xxx/cmds/") - 1;
    LOG_DBG("parsed sub topic is %s", sub_topic);
    if(*sub_topic == 'd')
    {
        LOG_DBG("guess for direction");
        int ret = json_obj_parse(payload, payload_length, dio_direction_json, ARRAY_SIZE(dio_direction_json), dio);
        LOG_DBG("ret=%d", ret);
        LOG_DBG("direction.value=%s", dio->direction.value);
        LOG_DBG("direction.pull=%s", dio->direction.pull);
        LOG_DBG("direction.polling_cycle=%d", dio->direction.polling_cycle);
        gpio_flags_t flags = 0;
        if(*dio->direction.value=='o')
        {
            flags = GPIO_OUTPUT;
        }
        else
        {
            flags = GPIO_INPUT;
        }
        gpio_pin_configure_dt(&dio->gpio, flags);
        // gpio_flags_t it;
        // if(!(flags & GPIO_INPUT))
        // {
        //     LOG_DBG("disable interrupt");
        //     it = GPIO_INT_DISABLE;
        // }
        // else
        // {
        //     LOG_DBG("enable interrupt");
        //     it = GPIO_INT_EDGE_BOTH;
        // }
        // gpio_pin_interrupt_configure_dt(&dio->gpio, it);
        panduza_dio_publish_direction(dio);
    }
    else if(*sub_topic == 's')
    {
        LOG_DBG("guess for state");
        int ret = json_obj_parse(payload, payload_length, dio_state_json, ARRAY_SIZE(dio_state_json), dio);
        LOG_DBG("ret=%d", ret);
        LOG_DBG("state.active=%d", dio->state.active);
        LOG_DBG("state.active_low=%d", dio->state.active_low);
        LOG_DBG("state.polling_cycle=%d", dio->state.polling_cycle);
        gpio_pin_set_dt(&dio->gpio, dio->state.active);
        panduza_dio_publish_state(dio);
    }
}