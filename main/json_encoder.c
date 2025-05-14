#include "cJSON.h"
#include "json_encoder.h"
#include <stdio.h>

char* encode_notification_message_json(const NotificationMessage *message) {
    if (message == NULL) return NULL;

    cJSON *root = cJSON_CreateObject();

    if (root == NULL) return NULL;

    char device_id_str[13];  // 6 byte * 2 + 1 null terminator
    snprintf(device_id_str, sizeof(device_id_str),
        "%02X%02X%02X%02X%02X%02X",
        message->device_id[0], message->device_id[1], message->device_id[2],
        message->device_id[3], message->device_id[4], message->device_id[5]);

    cJSON_AddStringToObject(root, "device_id", device_id_str);
    cJSON_AddNumberToObject(root, "therapy_id", message->therapy_id);
    cJSON_AddNumberToObject(root, "type", message->type);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;  // Dikkat: bu string heap'te, iş bitince free() etmeyi unutma
}


    