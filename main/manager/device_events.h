#ifndef DEVICE_EVENTS_H
#define DEVICE_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "device_configuration.h"
#include "../storage/log_resume.h"

typedef enum {
    EVT_NONE = 0,

    // Helmet / proximity
    EVT_HELMET_ON,
    EVT_HELMET_OFF,
    EVT_HELMET_OFF_DEBOUNCE_COMPLETED,

    // Alerts
    EVT_TEMPERATURE_ALERT,
    EVT_TEMPERATURE_NORMAL,

    // Timers
    EVT_INACTIVITY_TIMER_COMPLETED,
    EVT_THERAPY_TIMER_COMPLETED,
    EVT_ALERT_TIMER_COMPLETED,

    // Requests (BLE/app)
    EVT_ACTIVATION_REQUEST,
    EVT_STOP_REQUEST,
    EVT_PAUSE_REQUEST,
    EVT_CONTINUE_REQUEST,

    // Button
    EVT_SHORT_BUTTON_PRESS,
    EVT_LONG_BUTTON_PRESS,

    EVT_UNCOMPLETED_THERAPY_SET,
    EVT_DEVICE_START,
} DeviceEventType;

typedef struct {
    uint16_t duration_s;
    bool brightness_present;
    uint8_t brightness[TOTAL_REGION_COUNT]; 
} ActivationPayload;

typedef struct {
    uint8_t sensor_index;
} TempAlertPayload;

typedef struct {
    DeviceEventType type;

    union {
        UncompletedTherapyInfo uncompleted;
        ActivationPayload activation;
        TempAlertPayload temp_alert;
    } data;
} DeviceEvent;
#endif