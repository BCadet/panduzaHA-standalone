#include "panduza.h"
#include "app_mqtt.h"

#define PANDUZA_ITF_DIO_TYPE "dio"
#define PANDUZA_ITF_DIO_VERSION "1.0.0"

#define PANDUZA_IT_DIO_INFO "{\"type\": \""PANDUZA_ITF_DIO_TYPE"\",\"version\":\""PANDUZA_ITF_DIO_VERSION"\"}"


#include "app_mqtt.h"
extern app_mqtt_t app;

void panduza_publish_info()
{
    publish(&app.client, PANDUZA_TOPIC_BASE"/io/info", PANDUZA_IT_DIO_INFO, MQTT_QOS_0_AT_MOST_ONCE, true);
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