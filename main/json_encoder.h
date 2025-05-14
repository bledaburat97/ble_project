#include <stdint.h>
#include "notification.h"

#ifndef JSON_ENCODER_H
#define JSON_ENCODER_H

typedef struct {
    uint8_t device_id[6];      // 6 byte
    uint16_t therapy_id;       // 2 byte
    NotificationType type;     // 1 byte
} NotificationMessage;

char* encode_notification_message_json(const NotificationMessage *message);

#endif