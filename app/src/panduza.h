#ifndef PANDUZA_H
#define PANDUZA_H
#pragma once

#include "config.h"
#include <stdbool.h>
#define PANDUZA_TOPIC_BASE "pza/"CONFIG_PANDUZA_BENCH_NAME"/"CONFIG_PANDUZA_DEVICE_NAME"-dio-controller"

void panduza_publish_info(void);
void panduza_publish_identity();
void panduza_push_dio_direction(int interface, bool direction, bool pull);
void panduza_push_dio_update(int interface, bool state, bool active_low);
#endif