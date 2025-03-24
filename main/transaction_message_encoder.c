#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <esp_log.h>
#include <stddef.h>
#include "transaction_message_encoder.h"

static const char *TAG = "PeriodicInfoManager";

void encode_periodic_info(const PeriodicInfo *info, uint8_t *output) {
    output[0] = (info->therapy_id >> 8) & 0xFF;
    output[1] = info->therapy_id & 0xFF;

    output[2] = (info->remaining_duration >> 3) & 0xFF; // Remaining Duration (8-bit) 
    output[3] = ((info->remaining_duration & 0x07) << 5) | ((info->temperature >> 2) & 0x1F); // Remaining Duration (3-bit) + Temperature (5-bit)

    output[4] = (info->temperature & 0x03) << 6 | ((info->humidity >> 1) & 0x3F); // Temperature (2-bit) + Humidity (6-bit)
    output[5] = (info->humidity & 0x01) << 7 | 0x00; // Humidity (1-bit)
}

void encode_notification_info(const NotificationInfo *info, uint8_t *output) {
        output[0] = (info->therapy_id >> 8) & 0xFF;
        output[1] = info->therapy_id & 0xFF;
        output[2] = ((info->type & 0x0F) << 4) | 0x00; // Notification (4-bit)
}
