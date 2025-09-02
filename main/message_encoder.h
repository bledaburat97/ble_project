#include <stdint.h>
#include <stddef.h>

#ifndef MESSAGE_ENCODER_H
#define MESSAGE_ENCODER_H

#define DEVICE_INFO_SIZE 17
#define TIMER_STATE_INFO_SIZE 9
#define MEASUREMENT_INFO_SIZE 6
#define NOTIFICATION_INFO_SIZE 5

typedef struct {
    uint8_t device_id[6];
    uint8_t current_time[5];
    uint16_t last_saved_therapy_id;
    uint16_t message_id;
    uint16_t passed_seconds;
} DeviceInfoMessage;

typedef struct {
    uint8_t type;
    uint16_t therapy_id;
    uint16_t duration;
    uint16_t message_id;
    uint16_t passed_seconds;
} TimerStateInfoMessage;

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
    uint16_t message_id;
    uint16_t passed_seconds;
} MeasurementInfoMessage;

typedef struct {
    uint8_t type;
    uint16_t message_id;
    uint16_t passed_seconds;
} NotificationMessage;

char* encode_device_info_message(const DeviceInfoMessage *message);
char* encode_timer_state_info_message(const TimerStateInfoMessage *message);
char* encode_measurement_info_message(const MeasurementInfoMessage *message);
char* encode_notification_message(const NotificationMessage *message);
size_t encode_device_info_message_binary(const DeviceInfoMessage *m, uint8_t out[DEVICE_INFO_SIZE]);
size_t encode_timer_state_info_message_binary(const TimerStateInfoMessage *m, uint8_t out[TIMER_STATE_INFO_SIZE]);
size_t encode_measurement_info_message_binary(const MeasurementInfoMessage *m, uint8_t out[MEASUREMENT_INFO_SIZE]);
size_t encode_notification_message_binary(const NotificationMessage *m, uint8_t out[NOTIFICATION_INFO_SIZE]);

#endif