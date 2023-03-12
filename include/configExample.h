#ifndef __RGB_DINO_CONFIG_EXAMPLE_H__
#define __RGB_DINO_CONFIG_EXAMPLE_H__

#include <soc/soc.h>

// Debug
#define DEBUG          false // This will trigger other library's debug functionality
#define APP_DEBUG      false
#define APP_MQTT_DEBUG false

// Wifi
#define WLAN_SSID      ""
#define WLAN_PASS      ""

// MQTT
// 8883 === ssl
// 1883 === non-ssl
#define MQTT_URI       "mqtt://0.0.0.0:1883"
#define MQTT_USER      ""
#define MQTT_PASS      ""

// Subscription Topics
#define SUB_GET_COLOR  ""
#define SUB_SET_COLOR  ""

// Publish Topics
#define PUB_GET_COLOR  ""

// Pins
#define NEO_PIXEL_PIN   14
#define NEO_PIXEL_COUNT 12

// Mqtt overrides
// #define MQTT_CORE_SELECTION_ENABLED
// #define MQTT_TASK_CORE PRO_CPU_NUM

#endif
