
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <esp_log.h>
#include <stddef.h>

#include "laser_driver_controller.h"
#include "binary_message_parser.h"

static inline uint16_t be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

bool decode_activation_message_bin(const uint8_t *buf, ActivationMessage *out_msg) {
    if (!buf || !out_msg) return false;

    out_msg->duration = be16(&buf[0]);
    memcpy(out_msg->brightness, &buf[2], 6);
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