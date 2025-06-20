#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_MEASUREMENT_LOGS 676
#define MAX_NOTIFICATION_LOGS 1018
#define MAX_BRIGHTNESS_LOGS 406
#define MAX_LOG_ENTRY_SIZE 10

typedef enum {
    // General session events
    DEVICE_AWAKED = 0x01,
    BLE_CONNECTED,
    BLE_DISCONNECTED,

    // Therapy control events
    THERAPY_STARTED_BY_BUTTON,
    THERAPY_PAUSED_BY_BUTTON,
    THERAPY_CONTINUED_BY_BUTTON,
    THERAPY_COMPLETED,
    ONGOING_THERAPY,
    
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
    HELMET_OFF,
    HELMET_ON,

    // Timers
    THERAPY_TIMER_STOPPED,
    INACTIVITY_TIMER_STARTED,
    INACTIVITY_TIMER_EXPIRED,
    ALERT_TIMER_EXPIRED,

    // Errors and app events
    WRONG_PROX_MEASUREMENT,

    THERAPY_START_REQUEST,
    THERAPY_STARTED_BY_APP,
    THERAPY_CONTINUED_BY_APP,
    THERAPY_STOPPED_BY_APP,
    THERAPY_PAUSED_BY_APP,

    // Brightness
    REGIONS_BRIGHTNESS_UPDATED,

    // Time
    RTC_TIME_SAVED,
    PASSED_DURATION_UPDATED,
    POWER_IS_OFF,

    CURRENT_STATE_THERAPY,
    CURRENT_STATE_INACTIVITY,
    CURRENT_STATE_TEMP_ALERT,
    CURRENT_STATE_HUM_ALERT,
    
    DEVICE_INFO_MESSAGE_ACK,
    DEVICE_STATE_INFO_MESSAGE_ACK,
    HELMET_STATE_INFO_MESSAGE_ACK,
    ACTIVE_THERAPY_INFO_MESSAGE_ACK,
    RECORDS_INFO_FOR_ACTIVE_THERAPY_ACK,

    FLASH_SLOT_IS_FULL,
    TEST
} NotificationType;

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
    uint8_t type;
    uint8_t data[MAX_LOG_ENTRY_SIZE - 4];
    uint16_t passed_seconds;
    uint8_t crc;
    uint8_t entry_size;
    bool can_be_cached;
    bool can_be_flashed;
    bool can_start_cache;
    bool can_flush_cache;
} BaseLogEntry;

/* MessageType is normally defined in ble_control.h but that header depends on FreeRTOS.
 * Re-declare it here for host unit tests. */
typedef enum {
    RECORDS_INFO_MESSAGE,
    ACTIVE_THERAPY_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE
} MessageType;

/*
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

*/
