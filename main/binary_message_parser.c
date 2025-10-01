
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <esp_log.h>
#include <stddef.h>

#include "laser_driver_control.h"
#include "binary_message_parser.h"

static inline uint16_t be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

bool decode_activation_message_bin(const uint8_t *buf, ActivationMessage *out_msg) {
    if (!buf || !out_msg) return false;
    memcpy(out_msg->brightness, &buf[0], 6);
    out_msg->duration = be16(&buf[6]);
    return true;
}

bool decode_status_change_message_bin(const uint8_t *buf, StatusChangeMessage *out_msg) {
    if (!buf || !out_msg) return false;

    switch (buf[0]) {
        case PAUSE:
        case STOP:
        case CONTINUE:
            out_msg->type = (StatusChange)buf[0];
            return true;
        default:
            return false; // tanımsız tip
    }
}

bool decode_update_record_request_message_bin(const uint8_t *buf, UpdateRecordRequestMessage *out_msg) {
    if (!buf || !out_msg) return false;
    out_msg->last_therapy_id = be16(&buf[0]);
    return true;
}

bool decode_records_feedback_message_bin(const uint8_t *buf, RecordsFeedbackMessage *out_msg) {
    if (!buf || !out_msg) return false;
    out_msg->therapy_id = be16(&buf[0]);
    out_msg->is_success = (buf[2] != 0);
    return true;
}

bool decode_update_passkey_message_bin(const uint8_t *buf, UpdatePasskeyMessage *out_msg) {
    if (!buf || !out_msg) return false;
    out_msg->passkey = be16(&buf[0]);
    return true;
}

/*

bool decode_activation_message(const char* json_str, ActivationMessage* out_msg) {
    if (json_str == NULL || out_msg == NULL) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *dur = cJSON_GetObjectItem(root, "therapy_dur");
    cJSON *brightness = cJSON_GetObjectItem(root, "brightness");

    if (!cJSON_IsNumber(dur) || !cJSON_IsString(brightness)) {
        cJSON_Delete(root);
        return false;
    }

    out_msg->duration = (uint16_t) dur->valuedouble;

    // Brightness string: should be 12 hex characters = 6 bytes
    const char *hex_str = brightness->valuestring;
    if (strlen(hex_str) != 12) {
        cJSON_Delete(root);
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        char byte_str[3] = { hex_str[i * 2], hex_str[i * 2 + 1], '\0' };
        out_msg->brightness[i] = (uint8_t) strtol(byte_str, NULL, 16);
    }

    cJSON_Delete(root);
    return true;
}

bool decode_status_change_message(const char* json_str, StatusChangeMessage* out_msg) {
    if (json_str == NULL || out_msg == NULL) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *therapy_id = cJSON_GetObjectItem(root, "therapy_id");

    if (!cJSON_IsString(type) || !cJSON_IsNumber(therapy_id)) {
        cJSON_Delete(root);
        return false;
    }

    // Desteklenen string türleri ("PAUSE", "STOP", "CONTINUE")
    strncpy(out_msg->type, type->valuestring, sizeof(out_msg->type) - 1);
    out_msg->type[sizeof(out_msg->type) - 1] = '\0';  // null-terminate
    out_msg->therapy_id = (uint16_t) therapy_id->valuedouble;

    cJSON_Delete(root);
    return true;
}


bool decode_update_record_request_message(const char* json_str, UpdateRecordRequestMessage* out_msg) {
    if (json_str == NULL || out_msg == NULL) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *id = cJSON_GetObjectItem(root, "last_therapy_id");

    if (!cJSON_IsNumber(id)) {
        cJSON_Delete(root);
        return false;
    }

    out_msg->last_therapy_id = (uint16_t)id->valuedouble;

    cJSON_Delete(root);
    return true;
}


bool decode_feedback_message(const char* json_str, FeedbackMessage* out_msg) {
    if (json_str == NULL || out_msg == NULL) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *message_id = cJSON_GetObjectItem(root, "message_id");

    if (!cJSON_IsNumber(message_id)) {
        cJSON_Delete(root);
        return false;
    }

    out_msg->message_id = (uint8_t)message_id->valuedouble;

    cJSON_Delete(root);
    return true;
}

bool decode_records_feedback_message(const char* json_str, RecordsFeedbackMessage* out_msg) {
    if (json_str == NULL || out_msg == NULL) return false;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) return false;

    cJSON *id = cJSON_GetObjectItem(root, "therapy_id");
    cJSON *success = cJSON_GetObjectItem(root, "is_success");

    if (!cJSON_IsNumber(id) || !cJSON_IsNumber(success)) {
        cJSON_Delete(root);
        return false;
    }

    out_msg->therapy_id = (uint16_t)id->valuedouble;
    out_msg->is_success = (bool) id->valueint > 0;

    cJSON_Delete(root);
    return true;
}

*/