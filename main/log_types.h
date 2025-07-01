#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAX_MEASUREMENT_LOGS 676
#define MAX_NOTIFICATION_LOGS 1018
#define MAX_BRIGHTNESS_LOGS 406
#define MAX_LOG_ENTRY_SIZE 10

typedef enum {
    TIMER_STATE_INACTIVE = 0x01, //Zamanlayıcı Durumu 
    TIMER_STATE_ACTIVE_THERAPY, //Zamanlayıcı Durumu
    TIMER_STATE_PAUSED_THERAPY, //Zamanlayıcı Durumu
    TIMER_STATE_NEW_THERAPY_BY_BUTTON, //Zamanlayıcı Durumu
    TIMER_STATE_NEW_THERAPY_BY_APP, //Zamanlayıcı Durumu
    TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON, //Zamanlayıcı Durumu
    TIMER_STATE_CONTINUE_THERAPY_BY_APP, //Zamanlayıcı Durumu
    TIMER_STATE_LOW_TEMP_ALERT_1, //Zamanlayıcı Durumu
    TIMER_STATE_HIGH_TEMP_ALERT_1, //Zamanlayıcı Durumu
    TIMER_STATE_LOW_TEMP_ALERT_2, //Zamanlayıcı Durumu
    TIMER_STATE_HIGH_TEMP_ALERT_2, //Zamanlayıcı Durumu
    TIMER_STATE_LOW_TEMP_ALERT_3, //Zamanlayıcı Durumu
    TIMER_STATE_HIGH_TEMP_ALERT_3, //Zamanlayıcı Durumu
    TIMER_STATE_LOW_HUM_ALERT, //Zamanlayıcı Durumu 
    TIMER_STATE_HIGH_HUM_ALERT, //Zamanlayıcı Durumu 

    NOTIF_THERAPY_PAUSED_BY_BUTTON,  //Bildirim
    NOTIF_SHUT_DOWN_BY_BUTTON, //Bildirim
    NOTIF_THERAPY_STOPPED_BY_APP, //Bildirim
    NOTIF_THERAPY_PAUSED_BY_APP,  //Bildirim
    NOTIF_HELMET_OFF, //Bildirim
    NOTIF_HELMET_ON, //Bildirim
    NOTIF_BRIGHTNESS_UPDATED, //Bildirim
    NOTIF_THERAPY_COMPLETED,  //Bildirim
    NOTIF_INACTIVITY_TIMER_EXPIRED, //Bildirim
    NOTIF_ALERT_TIMER_EXPIRED, //Bildirim

    DEVICE_AWAKED,
    BLE_CONNECTED,
    BLE_DISCONNECTED,
    MEASUREMENT_CHANGED,
    INACTIVITY_TIMER_STARTED,
    HIGH_PROX_DETECTED_1,
    LOW_PROX_DETECTED_1,
    HIGH_PROX_DETECTED_2,
    LOW_PROX_DETECTED_2,
    WRONG_PROX_MEASUREMENT,
    RTC_TIME_SAVED, //henüz implemente edilmedi
    PASSED_DURATION_UPDATED, 
    FLASH_SLOT_IS_FULL, 
    TEST,
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
