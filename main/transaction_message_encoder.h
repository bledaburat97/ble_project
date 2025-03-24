#include <stdint.h>

#ifndef TRANSACTION_MESSAGE_ENCODER_H
#define TRANSACTION_MESSAGE_ENCODER_H
#define SIZE_OF_PERIODIC_INFO 6
#define SIZE_OF_NOTIFICATION_INFO 3
typedef struct {
    uint16_t therapy_id;            // 2 byte
    uint16_t remaining_duration;    // 11 bit
    uint8_t temperature;            // 7 bit
    uint8_t humidity;               // 7 bit
} PeriodicInfo;

typedef struct {
    uint16_t therapy_id;            // 2 byte
    uint8_t type;                   // 4 bit
} NotificationInfo;

void encode_periodic_info(const PeriodicInfo *info, uint8_t *output);
void encode_notification_info(const NotificationInfo *info, uint8_t *output);

#endif