/*
 * Copyright (c) 2025 nRFModule
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRFMODULE_MQTT_H
#define NRFMODULE_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

/** @brief MQTT events notified to the application. */
enum nrfmodule_mqtt_evt_type {
    NRFMODULE_MQTT_EVT_CONNACK,
    NRFMODULE_MQTT_EVT_DISCONNECT,
    NRFMODULE_MQTT_EVT_PUBLISH,
    NRFMODULE_MQTT_EVT_PUBACK,
    NRFMODULE_MQTT_EVT_PUBREC,
    NRFMODULE_MQTT_EVT_PUBREL,
    NRFMODULE_MQTT_EVT_PUBCOMP,
    NRFMODULE_MQTT_EVT_SUBACK,
    NRFMODULE_MQTT_EVT_UNSUBACK,
    NRFMODULE_MQTT_EVT_PINGRESP,
};

/** @brief UTF-8 encoded string structure. */
struct nrfmodule_mqtt_utf8 {
    const char *utf8;
    uint32_t size;
};

/** @brief Binary string structure. */
struct nrfmodule_mqtt_binstr {
    uint8_t *data;
    uint32_t len;
};

/** @brief MQTT topic structure. */
struct nrfmodule_mqtt_topic {
    struct nrfmodule_mqtt_utf8 topic;
    uint8_t qos;
};

/** @brief MQTT publish message structure. */
struct nrfmodule_mqtt_publish_message {
    struct nrfmodule_mqtt_topic topic;
    struct nrfmodule_mqtt_binstr payload;
};

/** @brief MQTT publish parameters. */
struct nrfmodule_mqtt_publish_param {
    struct nrfmodule_mqtt_publish_message message;
    uint16_t message_id;
    uint8_t dup_flag;
    uint8_t retain_flag;
};

/** @brief MQTT event structure. */
struct nrfmodule_mqtt_evt {
    enum nrfmodule_mqtt_evt_type type;
    int result;
    union {
        struct nrfmodule_mqtt_publish_message publish;
    } param;
};

struct nrfmodule_mqtt_client;

/** @brief MQTT event callback. */
typedef void (*nrfmodule_mqtt_evt_cb_t)(struct nrfmodule_mqtt_client *client,
                                        const struct nrfmodule_mqtt_evt *evt);

/** @brief MQTT client structure. */
struct nrfmodule_mqtt_client {
    struct nrfmodule_mqtt_utf8 client_id;
    struct nrfmodule_mqtt_utf8 broker;
    struct nrfmodule_mqtt_utf8 *user_name;
    struct nrfmodule_mqtt_utf8 *password;
    uint16_t port;
    uint32_t keepalive;
    bool clean_session;
    int sec_tag; /* -1 for insecure */
    
    bool is_connected;
    nrfmodule_mqtt_evt_cb_t evt_cb;
};

/** @brief MQTT subscription list item. */
struct nrfmodule_mqtt_topic_list {
    struct nrfmodule_mqtt_utf8 topic;
    uint8_t qos;
};

/** @brief MQTT subscription list. */
struct nrfmodule_mqtt_subscription_list {
    struct nrfmodule_mqtt_topic_list *list;
    uint16_t list_count;
    uint16_t message_id;
};

/**
 * @brief Initialize the MQTT client.
 *
 * @param client Client structure.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_init(struct nrfmodule_mqtt_client *client);

/**
 * @brief Connect to the MQTT broker.
 *
 * @param client Client structure.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_connect(struct nrfmodule_mqtt_client *client);

/**
 * @brief Disconnect from the MQTT broker.
 *
 * @param client Client structure.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_disconnect(struct nrfmodule_mqtt_client *client);

/**
 * @brief Publish a message.
 *
 * @param client Client structure.
 * @param param Publish parameters.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_publish(struct nrfmodule_mqtt_client *client,
                           const struct nrfmodule_mqtt_publish_param *param);

/**
 * @brief Subscribe to topics.
 *
 * @param client Client structure.
 * @param param Subscription list.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_subscribe(struct nrfmodule_mqtt_client *client,
                             const struct nrfmodule_mqtt_subscription_list *param);

/**
 * @brief Unsubscribe from topics.
 *
 * @param client Client structure.
 * @param param Subscription list.
 * @return 0 on success, negative errno on failure.
 */
int nrfmodule_mqtt_unsubscribe(struct nrfmodule_mqtt_client *client,
                               const struct nrfmodule_mqtt_subscription_list *param);

#ifdef __cplusplus
}
#endif

#endif /* NRFMODULE_MQTT_H */

