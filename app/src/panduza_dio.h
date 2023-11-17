#ifndef PANDUZA_DIO_H
#define PANDUZA_DIO_H
#pragma once

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

#endif