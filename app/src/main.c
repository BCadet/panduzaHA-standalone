/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(panduzaHA_standalone, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>

#include <zephyr/random/random.h>

#include <string.h>
#include <errno.h>

#include "config.h"
#include "panduza.h"

#include <zephyr/drivers/gpio.h>
#define SW0_NODE DT_ALIAS(sw0)
#define NODE DT_NODELABEL(user_button)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(NODE, gpios, {0});
static struct gpio_callback button_cb_data;

#include "app_mqtt.h"

extern app_mqtt_t app;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	for(int i=0; i<32; i++)
	{
		if (pins & BIT(i))
		{
			gpio_flags_t flags = 0;
			gpio_pin_get_config(dev, i, &flags);
			LOG_INF("flags=%08x", flags);
			panduza_push_dio_update(i, gpio_pin_get(dev, i), flags&GPIO_OUTPUT_INIT_LOW);
			panduza_push_dio_direction(i, flags&GPIO_OUTPUT_INIT_LOGICAL, flags&GPIO_PULL_UP);
			return;
		}
	}
}

static int start_app(void)
{
	int r = 0, i = 0;

	while (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS ||
	       i++ < CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
		r = publisher();

		if (!CONFIG_NET_SAMPLE_APP_MAX_CONNECTIONS) {
			k_sleep(K_MSEC(5000));
		}
	}

	return r;
}

#if defined(CONFIG_USERSPACE)
#define STACK_SIZE 2048

#if defined(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#else
#define THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif

K_THREAD_DEFINE(app_thread, STACK_SIZE,
		start_app, NULL, NULL, NULL,
		THREAD_PRIORITY, K_USER, -1);

static K_HEAP_DEFINE(app_mem_pool, 1024 * 2);
#endif

int main(void)
{
	int ret;
	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return 0;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return 0;
	}
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
#if defined(CONFIG_MQTT_LIB_TLS)
	int rc;

	rc = tls_init();
	PRINT_RESULT("tls_init", rc);
#endif

#if defined(CONFIG_USERSPACE)
	int ret;

	struct k_mem_partition *parts[] = {
#if Z_LIBC_PARTITION_EXISTS
		&z_libc_partition,
#endif
		&app_partition
	};

	ret = k_mem_domain_init(&app_domain, ARRAY_SIZE(parts), parts);
	__ASSERT(ret == 0, "k_mem_domain_init() failed %d", ret);
	ARG_UNUSED(ret);

	k_mem_domain_add_thread(&app_domain, app_thread);
	k_thread_heap_assign(app_thread, &app_mem_pool);

	k_thread_start(app_thread);
	k_thread_join(app_thread, K_FOREVER);
#else
	exit(start_app());
#endif
	return 0;
}
