#ifndef PANDUZA_DIO_H
#define PANDUZA_DIO_H
#pragma once

#include <zephyr/drivers/gpio.h>

#define PANDUZA_DIO_TYPE "dio"
#define PANDUZA_DIO_VERSION "1.0.0"
#define PANDUZA_DIO_INFO "{\"info\":{\"type\": \""PANDUZA_DIO_TYPE"\",\"version\":\""PANDUZA_DIO_VERSION"\",\"state\":\"run\"}}"

typedef struct dio_state {
    bool active;
    bool active_low;
    int polling_cycle;
} dio_state_t;

typedef struct dio_direction {
    char* value;
    char* pull;
    int polling_cycle;
} dio_direction_t;

typedef struct panduza_dio
{
    struct gpio_dt_spec gpio;
    char bank;
    dio_state_t state;
    dio_direction_t direction;
} dio_t;

void panduza_dio_publish_direction(dio_t *dio);
void panduza_dio_publish_state(dio_t *dio);
void panduza_dio_publish_info(dio_t *dio);

void pza_dio_init();
void pza_dio_publish_all();
void pza_dio_connect();

#endif