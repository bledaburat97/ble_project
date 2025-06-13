#include <stdint.h>
#include "log_types.h"

#ifndef JSON_ENCODER_H
#define JSON_ENCODER_H

typedef struct {
    uint8_t device_id[6];
    uint8_t current_time[5];
    uint16_t passed_seconds;
    uint16_t last_saved_therapy_id;
} DeviceInfoMessage;

typedef struct {
    uint8_t type;
    uint16_t therapy_id;
    uint16_t therapy_dur;
    uint16_t passed_seconds;
} TherapyActivationInfoMessage;

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint16_t passed_seconds;
} MeasurementInfoMessage;

typedef struct {
    uint8_t type;
    uint16_t passed_seconds;
} NotificationMessage;

char* encode_device_info_message(const DeviceInfoMessage *message);
char* encode_therapy_activation_info_message(const TherapyActivationInfoMessage *message);
char* encode_measurement_info_message(const MeasurementInfoMessage *message);
char* encode_notification_message(const NotificationMessage *message);

#endif