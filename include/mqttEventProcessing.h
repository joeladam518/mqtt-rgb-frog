#ifndef __RGB_DINO_MQTT_PROCESSING_H__
#define __RGB_DINO_MQTT_PROCESSING_H__

#include "config.h"
#include <mqtt_client.h>

#define SUBSCRIPTIONDATALEN 100
#define READ_SUBSCRIPTION_TIMEOUT 2000

typedef void (*SubscribeCallbackBufferType)(char *str, uint16_t len);

typedef enum MqttQos {
    QOS_AT_MOST_ONCE = 0,
    QOS_AT_LEAST_ONCE = 1,
    QOS_EXACTLY_ONCE = 2,
} MqttQos_t;

typedef enum SubsctiptionActionType {
    UNKNOWN = 0,
    GET_COLOR = 1,
    SET_COLOR = 2,
} SubsctiptionActionType_t;

typedef struct SubscriptionAction {
    esp_mqtt_client_handle_t client;
    SubsctiptionActionType_t type;
    char data[SUBSCRIPTIONDATALEN];
    uint16_t dataLength;
} SubscriptionAction_t;

// Subscribe callbacks
void getColor(SubscriptionAction_t *action);
void setColor(SubscriptionAction_t *action);

// Publish functions
void publishRgbStatus(void);

// Task functions
void processShortTask(void *parameter);
void processLongTask(void *parameter);

// Mqtt functions
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#endif
