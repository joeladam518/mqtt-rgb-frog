#ifndef __RGB_DINO_GLOBALS_H__
#define __RGB_DINO_GLOBALS_H__

#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <mqtt_client.h>
#include <TinyPICO.h>
#include "neoPixelRing.h"

//==============================================================================
// Macros

#define APP_FAIL_IF(condition, message) {\
    if (condition) {\
        Serial.println(message);\
        while(1);\
    }\
}

//==============================================================================
// Global Variables

// Mqtt client
extern esp_mqtt_client_handle_t mqttClient;
// TIny pico helper
extern TinyPICO pico;
// Adafruit NeoPixels
extern NeoPixelRing ring;
// freertos
extern TaskHandle_t mqttMTaskHandle;
extern TaskHandle_t processShortTaskHandle;
extern TaskHandle_t processLongTaskHandle;
extern QueueHandle_t shortActionQueue;
extern QueueHandle_t longActionQueue;
extern SemaphoreHandle_t ringMutex;

#endif
