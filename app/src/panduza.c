#include "panduza.h"
#include "app_mqtt.h"

#define PANDUZA_ITF_DIO_TYPE "dio-controller"
#define PANDUZA_ITF_DIO_VERSION "1.0.0"
#define PANDUZA_TIF_DIO_NUMBER_OF_IO "3"
#define PANDUZA_ITF_DIO_INFO "{\"info\":{\"type\": \""PANDUZA_ITF_DIO_TYPE"\",\"version\":\""PANDUZA_ITF_DIO_VERSION"\",\"state\":\"run\",\"number_of_interfaces\":"PANDUZA_TIF_DIO_NUMBER_OF_IO"}}"
#define PANDUZA_ITF_DIO_IDENTITY "{\"identity\":{\"family\": \"DIO\",\"model\":\"generic\",\"manufacturer\":\"Panduza\"}}"

#define PANDUZA_DIO_TYPE "dio"
#define PANDUZA_DIO_VERSION "1.0.0"
#define PANDUZA_DIO_INFO "{\"info\":{\"type\": \""PANDUZA_DIO_TYPE"\",\"version\":\""PANDUZA_DIO_VERSION"\",\"state\":\"run\"}}"

#include "app_mqtt.h"
extern app_mqtt_t app;

void panduza_publish_info()
{
    publish(&app.client, PANDUZA_TOPIC_BASE"/dio_13/atts/info", PANDUZA_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
    publish(&app.client, PANDUZA_TOPIC_BASE"/device/atts/info", PANDUZA_ITF_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void panduza_publish_identity()
{
    // publish(&app.client, PANDUZA_TOPIC_BASE"/io/device/atts/identity", PANDUZA_ITF_DIO_IDENTITY, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void panduza_handle_cmds(char* cmd, char *payload)
{
    if(strcmp(cmd, "direction") == 0)
    {

    }
    else if(strcmp(cmd, "state") == 0)
    {
        
    }
}
#include <zephyr/data/json.h>
#include "panduza_dio.h"

void panduza_push_dio_direction(int interface, bool direction, bool pull)
{
    struct dio_json_struct {
		dio_direction_t direction;
	} io_json = {0};

    const struct json_obj_descr io_direction_json_desc[] = {
		JSON_OBJ_DESCR_PRIM(dio_direction_t, value, JSON_TOK_STRING),
		JSON_OBJ_DESCR_PRIM(dio_direction_t, pull, JSON_TOK_STRING),
		JSON_OBJ_DESCR_PRIM(dio_direction_t, polling_cycle, JSON_TOK_NUMBER),
	};
	const struct json_obj_descr io_json_descr[] = {
		JSON_OBJ_DESCR_OBJECT(struct dio_json_struct, direction, io_direction_json_desc)
	};
	unsigned char json_encoded_buf[1024];

    io_json.direction.value = direction?"out":"in";
    io_json.direction.pull = pull?"up":"down";
    io_json.direction.polling_cycle = 1;
    int status = json_obj_encode_buf(io_json_descr, ARRAY_SIZE(io_json_descr),
                &io_json, json_encoded_buf, sizeof(json_encoded_buf));
    char topic[] = PANDUZA_TOPIC_BASE"/dio_xx/atts/direction";
    topic[strlen(topic)-17] = '0' + interface/10;
    topic[strlen(topic)-16] = '0' + interface%10;
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, false);

}

void panduza_push_dio_update(int interface, bool state, bool active_low)
{
	struct dio_json_struct {
		dio_state_t state;
	} io_json = {0};

	const struct json_obj_descr io_state_json_desc[] = {
		JSON_OBJ_DESCR_PRIM(dio_state_t, active, JSON_TOK_TRUE),
		JSON_OBJ_DESCR_PRIM(dio_state_t, active_low, JSON_TOK_TRUE),
		JSON_OBJ_DESCR_PRIM(dio_state_t, polling_cycle, JSON_TOK_NUMBER),
	};
	const struct json_obj_descr io_json_descr[] = {
		JSON_OBJ_DESCR_OBJECT(struct dio_json_struct, state, io_state_json_desc)
	};
	unsigned char json_encoded_buf[1024];

    io_json.state.active = state;
    io_json.state.active_low = active_low;
    io_json.state.polling_cycle = 1;
    int status = json_obj_encode_buf(io_json_descr, ARRAY_SIZE(io_json_descr),
                &io_json, json_encoded_buf, sizeof(json_encoded_buf));
    char topic[] = PANDUZA_TOPIC_BASE"/dio_xx/atts/state";
    topic[strlen(topic)-13] = '0' + interface/10;
    topic[strlen(topic)-12] = '0' + interface%10;
    publish(&app.client, topic, json_encoded_buf, MQTT_QOS_0_AT_MOST_ONCE, false);
}

void panduza_server_publish_info()
{
    publish(&app.client, "pza/server/"CONFIG_PANDUZA_DEVICE_NAME"/device/atts/info", PANDUZA_ITF_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
    publish(&app.client, "pza/server/"CONFIG_PANDUZA_DEVICE_NAME"/device/atts/identity", PANDUZA_ITF_DIO_IDENTITY, MQTT_QOS_0_AT_MOST_ONCE, true);
}