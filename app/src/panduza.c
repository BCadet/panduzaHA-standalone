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

extern app_mqtt_t app;

void panduza_publish_identity()
{
    // publish(&app.client, PANDUZA_TOPIC_BASE"/io/device/atts/identity", PANDUZA_ITF_DIO_IDENTITY, MQTT_QOS_0_AT_MOST_ONCE, true);
}

void panduza_server_publish_info()
{
    publish(&app.client, "pza/server/"CONFIG_PANDUZA_DEVICE_NAME"/device/atts/info", PANDUZA_ITF_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
    publish(&app.client, "pza/server/"CONFIG_PANDUZA_DEVICE_NAME"/device/atts/identity", PANDUZA_ITF_DIO_IDENTITY, MQTT_QOS_0_AT_MOST_ONCE, true);
}