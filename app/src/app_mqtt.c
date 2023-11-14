#include <zephyr/net/mqtt.h>
#include <zephyr/random/random.h>
#include <zephyr/data/json.h>
#include "panduza.h"

#include "app_mqtt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_mqtt, LOG_LEVEL_DBG);

#if defined(CONFIG_USERSPACE)
#include <zephyr/app_memory/app_memdomain.h>
K_APPMEM_PARTITION_DEFINE(app_partition);
struct k_mem_domain app_domain;
#define APP_BMEM K_APP_BMEM(app_partition)
#define APP_DMEM K_APP_DMEM(app_partition)
#else
#define APP_BMEM
#define APP_DMEM
#endif

APP_BMEM app_mqtt_t app;

#if defined(CONFIG_MQTT_LIB_WEBSOCKET)
/* Making RX buffer large enough that the full IPv6 packet can fit into it */
#define MQTT_LIB_WEBSOCKET_RECV_BUF_LEN 1280

/* Websocket needs temporary buffer to store partial packets */
static APP_BMEM uint8_t temp_ws_rx_buf[MQTT_LIB_WEBSOCKET_RECV_BUF_LEN];
#endif

#if defined(CONFIG_SOCKS)
static APP_BMEM struct sockaddr socks5_proxy;
#endif

static void broker_init(struct sockaddr_storage *broker)
{
#if defined(CONFIG_NET_IPV6)
	struct sockaddr_in6 *broker6 = (struct sockaddr_in6 *)&broker;

	broker6->sin6_family = AF_INET6;
	broker6->sin6_port = htons(SERVER_PORT);
	zsock_inet_pton(AF_INET6, SERVER_ADDR, &broker6->sin6_addr);

#if defined(CONFIG_SOCKS)
	struct sockaddr_in6 *proxy6 = (struct sockaddr_in6 *)&socks5_proxy;

	proxy6->sin6_family = AF_INET6;
	proxy6->sin6_port = htons(SOCKS5_PROXY_PORT);
	zsock_inet_pton(AF_INET6, SOCKS5_PROXY_ADDR, &proxy6->sin6_addr);
#endif
#else
	struct sockaddr_in *broker4 = (struct sockaddr_in *)broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port = htons(SERVER_PORT);
	zsock_inet_pton(AF_INET, SERVER_ADDR, &broker4->sin_addr);
#if defined(CONFIG_SOCKS)
	struct sockaddr_in *proxy4 = (struct sockaddr_in *)&socks5_proxy;

	proxy4->sin_family = AF_INET;
	proxy4->sin_port = htons(SOCKS5_PROXY_PORT);
	zsock_inet_pton(AF_INET, SOCKS5_PROXY_ADDR, &proxy4->sin_addr);
#endif
#endif
}

struct test_json
{
	int test;
	char* toto;
};

const struct json_obj_descr obj_array_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct test_json, test, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct test_json, toto, JSON_TOK_STRING),
};

void mqtt_evt_handler(struct mqtt_client *const client,
		      const struct mqtt_evt *evt)
{
	int err;
	unsigned char payload[1024];
	struct test_json out;

	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT connect failed %d", evt->result);
			break;
		}

		app.connected = true;
		LOG_INF("MQTT client connected!");

		break;

	case MQTT_EVT_DISCONNECT:
		LOG_INF("MQTT client disconnected %d", evt->result);

		app.connected = false;
		clear_fds();

		break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);

		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
			evt->param.pubcomp.message_id);

		break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	case MQTT_EVT_PUBLISH:
		LOG_INF("PUBLISH event");
		LOG_INF("topic=%s [%d]", evt->param.publish.message.topic.topic.utf8, evt->param.publish.message.payload.len);
		mqtt_read_publish_payload_blocking(&app.client, payload, evt->param.publish.message.payload.len);
		// payload[evt->param.publish.message.payload.len] = '\0';
		err = json_obj_parse(payload, evt->param.publish.message.payload.len, obj_array_descr, ARRAY_SIZE(obj_array_descr), &out);
		LOG_INF("err=%d/%d struct out.test=%d out.toto=%s", __builtin_popcount(err), ARRAY_SIZE(obj_array_descr), out.test, out.toto);
		break;

	case MQTT_EVT_SUBACK:
		LOG_INF("SUBACK event");
		break;

	default:
		break;
	}
}

static void app_mqtt_init()
{
	mqtt_client_init(&app.client);

	broker_init(&app.broker);

	/* MQTT client configuration */
	app.client.broker = &app.broker;
	app.client.evt_cb = mqtt_evt_handler;
	app.client.client_id.utf8 = (uint8_t *)MQTT_CLIENTID;
	app.client.client_id.size = strlen(MQTT_CLIENTID);
	app.client.password = NULL;
	app.client.user_name = NULL;
	app.client.protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	app.client.rx_buf = app.rx_buffer;
	app.client.rx_buf_size = sizeof(app.rx_buffer);
	app.client.tx_buf = app.tx_buffer;
	app.client.tx_buf_size = sizeof(app.tx_buffer);

	/* MQTT transport configuration */
#if defined(CONFIG_MQTT_LIB_TLS)
#if defined(CONFIG_MQTT_LIB_WEBSOCKET)
	app.client.transport.type = MQTT_TRANSPORT_SECURE_WEBSOCKET;
#else
	app.client.transport.type = MQTT_TRANSPORT_SECURE;
#endif

	struct mqtt_sec_config *tls_config = &app.client.transport.tls.config;

	tls_config->peer_verify = TLS_PEER_VERIFY_REQUIRED;
	tls_config->cipher_list = NULL;
	tls_config->sec_tag_list = m_sec_tags;
	tls_config->sec_tag_count = ARRAY_SIZE(m_sec_tags);
#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(CONFIG_NET_SOCKETS_OFFLOAD)
	tls_config->hostname = TLS_SNI_HOSTNAME;
#else
	tls_config->hostname = NULL;
#endif

#else
#if defined(CONFIG_MQTT_LIB_WEBSOCKET)
	app.client.transport.type = MQTT_TRANSPORT_NON_SECURE_WEBSOCKET;
#else
	app.client.transport.type = MQTT_TRANSPORT_NON_SECURE;
#endif
#endif

#if defined(CONFIG_MQTT_LIB_WEBSOCKET)
	app.client.transport.websocket.config.host = SERVER_ADDR;
	app.client.transport.websocket.config.url = "/mqtt";
	app.client.transport.websocket.config.tmp_buf = temp_ws_rx_buf;
	app.client.transport.websocket.config.tmp_buf_len =
						sizeof(temp_ws_rx_buf);
	app.client.transport.websocket.timeout = 5 * MSEC_PER_SEC;
#endif

#if defined(CONFIG_SOCKS)
	mqtt_client_set_proxy(client, &socks5_proxy,
			      socks5_proxy.sa_family == AF_INET ?
			      sizeof(struct sockaddr_in) :
			      sizeof(struct sockaddr_in6));
#endif
}

#if defined(CONFIG_MQTT_LIB_TLS)

#include "test_certs.h"

#define TLS_SNI_HOSTNAME "localhost"
#define APP_CA_CERT_TAG 1
#define APP_PSK_TAG 2

static APP_DMEM sec_tag_t m_sec_tags[] = {
#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(CONFIG_NET_SOCKETS_OFFLOAD)
		APP_CA_CERT_TAG,
#endif
#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
		APP_PSK_TAG,
#endif
};

static int tls_init(void)
{
	int err = -EINVAL;

#if defined(MBEDTLS_X509_CRT_PARSE_C) || defined(CONFIG_NET_SOCKETS_OFFLOAD)
	err = tls_credential_add(APP_CA_CERT_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				 ca_certificate, sizeof(ca_certificate));
	if (err < 0) {
		LOG_ERR("Failed to register public certificate: %d", err);
		return err;
	}
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
	err = tls_credential_add(APP_PSK_TAG, TLS_CREDENTIAL_PSK,
				 client_psk, sizeof(client_psk));
	if (err < 0) {
		LOG_ERR("Failed to register PSK: %d", err);
		return err;
	}

	err = tls_credential_add(APP_PSK_TAG, TLS_CREDENTIAL_PSK_ID,
				 client_psk_id, sizeof(client_psk_id) - 1);
	if (err < 0) {
		LOG_ERR("Failed to register PSK ID: %d", err);
	}
#endif

	return err;
}

#endif /* CONFIG_MQTT_LIB_TLS */

static void prepare_fds()
{
	if (app.client.transport.type == MQTT_TRANSPORT_NON_SECURE) {
		app.fds[0].fd = app.client.transport.tcp.sock;
	}
#if defined(CONFIG_MQTT_LIB_TLS)
	else if (client->transport.type == MQTT_TRANSPORT_SECURE) {
		fds[0].fd = client->transport.tls.sock;
	}
#endif

	app.fds[0].events = ZSOCK_POLLIN;
	app.nfds = 1;
}

void clear_fds(void)
{
	app.nfds = 0;
}

int wait(int timeout)
{
	int ret = 0;

	if (app.nfds > 0) {
		ret = zsock_poll(app.fds, app.nfds, timeout);
		if (ret < 0) {
			LOG_ERR("poll error: %d", errno);
		}
	}

	return ret;
}

static char *get_mqtt_payload(enum mqtt_qos qos)
{
	static APP_DMEM char payload[] = "{value: 0}";

	// payload[strlen(payload) - 2] = '0' + gpio_pin_get_dt(&button);

	return payload;
}

static char *get_mqtt_topic(uint8_t io_id)
{
	static char base[] = PANDUZA_TOPIC_BASE"/:io_XX_val";
	base[strlen(base)-6] = '0' + io_id/10;
	base[strlen(base)-5] = '0' + io_id%10;
	return base;
}

int publish(struct mqtt_client *client, const char* topic, const char* payload, enum mqtt_qos qos, bool retain)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (uint8_t *)topic;
	param.message.topic.topic.size =
			strlen(param.message.topic.topic.utf8);
	param.message.payload.data = payload;
	param.message.payload.len =
			strlen(param.message.payload.data);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0U;
	param.retain_flag = 0U;

	return mqtt_publish(client, &param);
}

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")

#define PRINT_RESULT(func, rc) \
	LOG_INF("%s: %d <%s>", (func), rc, RC_STR(rc))

/* In this routine we block until the connected variable is 1 */
static int try_to_connect(struct mqtt_client *client)
{
	int rc, i = 0;

	while (i++ < 5 && !app.connected) {

		app_mqtt_init();

		rc = mqtt_connect(&app.client);
		if (rc != 0) {
			PRINT_RESULT("mqtt_connect", rc);
			k_sleep(K_MSEC(500));
			continue;
		}

		prepare_fds(&app.client);

		if (wait(100)) {
			mqtt_input(&app.client);
		}

		if (!app.connected) {
			mqtt_abort(&app.client);
		}
	}

	if (app.connected) {
		struct mqtt_topic topics[] = {{
			.topic = {.utf8 = "test",
				.size = strlen("test")},
			.qos = MQTT_QOS_0_AT_MOST_ONCE,
		}};
		const struct mqtt_subscription_list sub_list = {
			.list = topics,
			.list_count = ARRAY_SIZE(topics),
			.message_id = 1u,
		};
		int ret = mqtt_subscribe(&app.client, &sub_list);
		if (ret != 0) {
			LOG_ERR("Failed to subscribe to topics: %d", ret);
		}
		return 0;
	}

	return -EINVAL;
}

static int process_mqtt_and_sleep(struct mqtt_client *client, int timeout)
{
	int64_t remaining = timeout;
	int64_t start_time = k_uptime_get();
	int rc;

	while (remaining > 0 && app.connected) {
		if (wait(remaining)) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		rc = mqtt_live(client);
		if (rc != 0 && rc != -EAGAIN) {
			PRINT_RESULT("mqtt_live", rc);
			return rc;
		} else if (rc == 0) {
			rc = mqtt_input(client);
			if (rc != 0) {
				PRINT_RESULT("mqtt_input", rc);
				return rc;
			}
		}

		remaining = timeout + start_time - k_uptime_get();
	}

	return 0;
}

#define SUCCESS_OR_EXIT(rc) { if (rc != 0) { return 1; } }
#define SUCCESS_OR_BREAK(rc) { if (rc != 0) { break; } }

int publisher(void)
{
	int i, rc, r = 0;

	LOG_INF("attempting to connect: ");
	rc = try_to_connect(&app.client);
	PRINT_RESULT("try_to_connect", rc);
	SUCCESS_OR_EXIT(rc);

	i = 0;
	while (app.connected) {
		r = -1;

		// rc = mqtt_ping(&client);
		// PRINT_RESULT("mqtt_ping", rc);
		// SUCCESS_OR_BREAK(rc);

		rc = process_mqtt_and_sleep(&app.client, 1);
		SUCCESS_OR_BREAK(rc);

		// rc = publish(&client, get_mqtt_topic(13), get_mqtt_payload(MQTT_QOS_0_AT_MOST_ONCE), MQTT_QOS_0_AT_MOST_ONCE, false);
		// PRINT_RESULT("mqtt_publish", rc);
		// SUCCESS_OR_BREAK(rc);
		// k_msleep(10);

		r = 0;
	}

	rc = mqtt_disconnect(&app.client);
	PRINT_RESULT("mqtt_disconnect", rc);

	LOG_INF("Bye!");

	return r;
}