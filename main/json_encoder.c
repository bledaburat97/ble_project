//can be deleted
#include "cJSON.h"
#include "message_encoder.h"
#include <stdio.h>

char* encode_device_info_message(const DeviceInfoMessage *message) {
    if (message == NULL) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    char device_id_str[13];  // 6 byte * 2 + 1 null terminator
    snprintf(device_id_str, sizeof(device_id_str),
        "%02X%02X%02X%02X%02X%02X",
        message->device_id[0], message->device_id[1], message->device_id[2],
        message->device_id[3], message->device_id[4], message->device_id[5]);

    char current_time_str[11];
    snprintf(current_time_str, sizeof(current_time_str),
        "%02X%02X%02X%02X%02X",
        message->current_time[0], message->current_time[1], message->current_time[2],
        message->current_time[3], message->current_time[4]);

    cJSON_AddStringToObject(root, "device_id", device_id_str);
    cJSON_AddStringToObject(root, "time", current_time_str);
    cJSON_AddNumberToObject(root, "last_therapy_id", message->last_saved_therapy_id);
    cJSON_AddNumberToObject(root, "passed_sec", message->passed_seconds);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;  // Dikkat: bu string heap'te, iş bitince free() etmeyi unutma
}

char* encode_timer_state_info_message(const TimerStateInfoMessage *message) {
    if (message == NULL) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "type", message->type);
    cJSON_AddNumberToObject(root, "therapy_id", message->therapy_id);
    cJSON_AddNumberToObject(root, "duration", message->duration);
    cJSON_AddNumberToObject(root, "passed_sec", message->therapy_passed_seconds);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

char* encode_measurement_info_message(const MeasurementInfoMessage *message) {
    if (message == NULL) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "t", message->temperature);
    cJSON_AddNumberToObject(root, "h", message->humidity);
    cJSON_AddNumberToObject(root, "p", message->passed_seconds);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

char* encode_notification_message(const NotificationMessage *message) {
    if (message == NULL) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;

    cJSON_AddNumberToObject(root, "type", message->type);
    cJSON_AddNumberToObject(root, "passed_sec", message->passed_seconds);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}