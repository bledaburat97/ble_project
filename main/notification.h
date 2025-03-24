#ifndef NOTIFICATION_H
#define NOTIFICATION_H

typedef enum {
    LOW_TEMPERATURE_ALARM = 0x00,
    HIGH_TEMPERATURE_ALARM = 0x01,
    LOW_HUMIDITY_ALARM = 0x02,
    HIGH_HUMIDITY_ALARM = 0x03,
    HELMET_OFF = 0x04,           // Helmet removed
    HELMET_ON = 0x05,            // Helmet worn
    THERAPY_COMPLETED = 0x06,
    INACIVITY_TIMER_COMPLETED = 0x07,
    AFTER_ALARM_TIMER_ENDED = 0x08,
    WRONG_TEMP_THRESHOLD_VALUES = 0x09
} NotificationType;

#endif 
