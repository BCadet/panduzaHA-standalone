#ifndef APP_MQTT_H
#define APP_MQTT_H
#pragma once

#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include "config.h"

typedef struct app_mqtt
{
	/* Buffers for MQTT client. */
	uint8_t rx_buffer[APP_MQTT_BUFFER_SIZE];
	uint8_t tx_buffer[APP_MQTT_BUFFER_SIZE];
	/* The mqtt client struct */
	struct mqtt_client client;
	/* MQTT Broker details. */
	struct sockaddr_storage broker;
	struct zsock_pollfd fds[1];
	int nfds;
	bool connected;
} app_mqtt_t;

int publish(struct mqtt_client *client, const char* topic, const char* payload, enum mqtt_qos qos, bool retain);
int publisher(void);
#endif