#include "config.h"
#include "globals.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <esp_log.h>
#include <mqtt_client.h>

#include "log.h"
#include "led.h"
#include "mqttEventProcessing.h"
#include "neoPixelRing.h"

//==============================================================================
// Helpers

static SubsctiptionActionType_t getActionType(esp_mqtt_event_handle_t event)
{
    if (strncmp(SUB_SET_COLOR, event->topic, event->topic_len) == 0) {
        return SET_COLOR;
    }

    if (strncmp(SUB_GET_COLOR, event->topic, event->topic_len) == 0) {
        return GET_COLOR;
    }

    return UNKNOWN;
}


static bool isShortTask(SubsctiptionActionType_t type) {
    if (type == GET_COLOR) {
        return true;
    }

    return false;
}

static bool isLongTask(SubsctiptionActionType_t type)
{
    if (type == SET_COLOR) {
        return true;
    }

    return false;
}

static void clearAction(SubscriptionAction_t *action)
{
    action->client = NULL;
    action->type = UNKNOWN;
    memset(action->data, '\0', SUBSCRIPTIONDATALEN);
    action->dataLength = 0;
}

static void setAction(
    SubscriptionAction_t *action,
    SubsctiptionActionType_t type,
    esp_mqtt_event_handle_t event
) {
    clearAction(action);

    if (SUBSCRIPTIONDATALEN < event->data_len) {
        return;
    }

    action->client = event->client;
    action->type = type;
    strncpy(action->data, (char *)event->data, event->data_len);
    action->dataLength = event->data_len;
}

//==============================================================================
// Mqtt publish functions

void publishRgbStatus(void)
{
    APP_LOG(F("publishRgbStatus()"));

    char output[SUBSCRIPTIONDATALEN];
    const int capacity = JSON_OBJECT_SIZE(3);
    StaticJsonDocument<capacity> doc;
    RGB_t color = {0, 0, 0};

    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        ring.getColor(&color);
        xSemaphoreGive(ringMutex);
    } else {
        APP_LOG(F("the ring is already taken"));
        return;
    }

    doc["r"] = color.r;
    doc["g"] = color.g;
    doc["b"] = color.b;

    serializeJson(doc, output, sizeof(output));
    esp_mqtt_client_publish(mqttClient, PUB_GET_COLOR, output, 0, 0, 0);
}

//==============================================================================
// Mqtt subscribe callback functions

void getColor(SubscriptionAction_t *action)
{
    APP_LOG(F("getColor()"));

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        APP_LOG(F("can't parse, data is to long..."));
        return;
    }

    publishRgbStatus();
}

void setColor(SubscriptionAction_t *action)
{
    APP_LOG(F("setColor()"));

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        APP_LOG(F("can't parse, data is to long..."));
        return;
    }

    const int capacity = JSON_OBJECT_SIZE(4);
    StaticJsonDocument<capacity> doc;
    DeserializationError error = deserializeJson(doc, action->data);

    if (error) {
        APP_LOG(&error);
        return;
    }

    uint8_t r = doc["r"].as<uint8_t>();
    uint8_t g = doc["g"].as<uint8_t>();
    uint8_t b = doc["b"].as<uint8_t>();
    uint16_t time = doc["time"].as<uint16_t>();

    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        if (time) {
            ring.fadeColor(r, g, b, time);
        } else {
            ring.setColor(r, g, b);
        }
        xSemaphoreGive(ringMutex);
    } else {
        APP_LOG(F("the ring is already taken"));
    }

    publishRgbStatus();
}

//==============================================================================
// Process Tasks

void processShortTask(void *parameter)
{
    SubscriptionAction_t action;

    while (1) {
        if (xQueueReceive(shortActionQueue, &action, 0) == pdTRUE) {
            APP_LOG(F("processShortTask()"));
            APP_LOG(&action);


            switch(action.type) {
                case GET_COLOR:
                    getColor(&action);
                    break;
            }

            clearAction(&action);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void processLongTask(void *parameter)
{
    SubscriptionAction_t action;

    while (1) {
        if (xQueueReceive(longActionQueue, &action, 0) == pdTRUE) {
            APP_LOG(F("processLongTask()"));
            APP_LOG(&action);

            switch(action.type) {
                case SET_COLOR:
                    setColor(&action);
                    break;
            }

            clearAction(&action);
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

//==============================================================================
// Mqtt event functions

static void mqtt_unsubscribe_all(esp_mqtt_client_handle_t client)
{
    esp_mqtt_client_unsubscribe(client, SUB_GET_COLOR);
    esp_mqtt_client_unsubscribe(client, SUB_SET_COLOR);
}

static void mqtt_subsribe_all(esp_mqtt_client_handle_t client)
{
    mqtt_unsubscribe_all(client);
    esp_mqtt_client_subscribe(client, SUB_GET_COLOR, QOS_AT_MOST_ONCE);
    esp_mqtt_client_subscribe(client, SUB_SET_COLOR, QOS_AT_MOST_ONCE);
}

static void mqtt_handle_data_event(esp_mqtt_event_handle_t event)
{
    APP_LOG(F("\nmqtt_handle_data_event()"));
    APP_LOG(event);

    SubscriptionAction_t action;
    SubsctiptionActionType_t actionType = getActionType(event);

    if (isShortTask(actionType)) {
        setAction(&action, actionType, event);
        if (action.type != UNKNOWN && action.client != NULL) {
            xQueueSend(shortActionQueue, &action, portMAX_DELAY);
        }
    } else if (isLongTask(actionType)) {
        setAction(&action, actionType, event);
        if (action.type != UNKNOWN && action.client != NULL) {
            xQueueSend(longActionQueue, &action, portMAX_DELAY);
        }
    } else {
        APP_LOG(F("Topic was unhandled"));
    }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            MQTT_EVENT_LOG(F("MQTT_EVENT_CONNECTED"));
            mqtt_subsribe_all(client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            MQTT_EVENT_LOG(F("MQTT_EVENT_DISCONNECTED"));
            break;
        case MQTT_EVENT_SUBSCRIBED:
            MQTT_EVENT_LOGF("MQTT_EVENT_SUBSCRIBED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            MQTT_EVENT_LOGF("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            MQTT_EVENT_LOGF("MQTT_EVENT_PUBLISHED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            mqtt_handle_data_event(event);
            break;
        case MQTT_EVENT_ERROR:
            MQTT_EVENT_LOG(F("MQTT_EVENT_ERROR"));
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                MQTT_EVENT_LOGF("Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            MQTT_EVENT_LOGF("Other event, id=%d\n", event->event_id);
            break;
    }
}
