#pragma once
#include <stdint.h>

#define MAX_MEASUREMENT_LOGS 676
#define MAX_NOTIFICATION_LOGS 1018
#define MAX_BRIGHTNESS_LOGS 406

typedef enum {
    // General session events
    WAKE_FROM_DEEP_SLEEP = 0x01,
    BLE_CONNECTED,
    BLE_DISCONNECTED,

    // Therapy control events
    THERAPY_STARTED_BY_BUTTON,
    THERAPY_PAUSED_BY_BUTTON,
    THERAPY_CONTINUED_BY_BUTTON,
    THERAPY_COMPLETED_LOG,

    // Temperature alerts
    LOW_TEMP_ALERT_1,
    HIGH_TEMP_ALERT_1,
    LOW_TEMP_ALERT_2,
    HIGH_TEMP_ALERT_2,
    LOW_TEMP_ALERT_3,
    HIGH_TEMP_ALERT_3,

    // Humidity alerts
    LOW_HUM_ALERT,
    HIGH_HUM_ALERT,

    // Measurement changes
    MEASUREMENT_CHANGED,

    // Proximity detection
    HIGH_PROX_DETECTED_1,
    LOW_PROX_DETECTED_1,
    HIGH_PROX_DETECTED_2,
    LOW_PROX_DETECTED_2,

    // Helmet events
    HELMET_OFF_LOG,
    HELMET_ON_LOG,

    // Timers
    THERAPY_TIMER_STOPPED,
    INACTIVITY_TIMER_STARTED,
    INACTIVITY_TIMER_EXPIRED,
    ALERT_TIMER_STARTED,
    ALERT_TIMER_CLEARED,

    // Errors and app events
    WRONG_TEMP_THRESHOLD,
    THERAPY_START_REQUEST,
    THERAPY_STARTED_BY_APP,
    THERAPY_CONTINUED_BY_APP,
    THERAPY_STOPPED_BY_APP,

    // Brightness
    REGIONS_BRIGHTNESS_UPDATED,

    // Time
    RTC_TIME_SAVED,
    TIME_UPDATED,
    POWER_IS_OFF,

    SLOT_IS_FULL
} change_type_t;

// RTC time save event
typedef struct {
    uint8_t type;
    uint8_t timestamp[5];
    uint16_t passed_seconds;
    uint8_t crc;
} __attribute__((packed)) Time_saved_t;

// Therapy start or continuation event
typedef struct {
    uint8_t type;
    uint16_t therapy_id;
    uint16_t therapy_duration;
    uint16_t passed_seconds;
    uint8_t crc;
} __attribute__((packed)) Therapy_initialization_t;


// MEASUREMENT_CHANGED event
typedef struct {
    uint8_t type;
    uint8_t temperature;
    uint8_t humidity;
    uint16_t passed_seconds;
    uint8_t crc;
} __attribute__((packed)) Measurement_changed_t;

// REGIONS_BRIGHTNESS_UPDATED event
typedef struct {
    uint8_t type;
    uint8_t region_brightnesses[6];
    uint16_t passed_seconds;
    uint8_t crc;
} __attribute__((packed)) Regions_updated_t;

// General notification event (all others)
typedef struct {
    uint8_t type;
    uint16_t passed_seconds;
    uint8_t crc;
} __attribute__((packed)) Notification_t;

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint16_t time;
} MeasurementChangeLog;

typedef struct {
    uint8_t humidity;
    uint16_t time;
} HumidityChangeLog;

typedef struct {
    uint8_t type;
    uint16_t time;
} NotificationLog;

typedef struct {
    uint8_t brightness[6];
    uint16_t time;
} BrightnessChangeLog;

typedef struct {
    uint16_t therapy_id;
    uint8_t device_id[6];
    uint8_t start_time[5]; // null olabilir. 
    uint16_t duration;
    uint16_t remaining_dur;
    MeasurementChangeLog measurement_logs[MAX_MEASUREMENT_LOGS];
    int measurement_log_count;
    NotificationLog notif_logs[MAX_NOTIFICATION_LOGS];
    int notif_log_count;
    BrightnessChangeLog brightness_logs[MAX_BRIGHTNESS_LOGS];
    int brightness_log_count;
} TherapySession;


