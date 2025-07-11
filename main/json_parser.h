
#include <stdint.h>
#include <stdbool.h>

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct {
    uint16_t duration;
    uint8_t brightness[6];
} ActivationMessage;

typedef struct {
    char type[10]; // "PAUSE", "STOP", "CONTINUE"
    uint16_t therapy_id;
} StatusChangeMessage;

typedef struct {
    uint16_t last_therapy_id;
} UpdateRecordRequestMessage;

typedef struct {
    uint8_t message_id;
} FeedbackMessage;

typedef struct {
    uint16_t therapy_id;
    bool is_success;
} RecordsFeedbackMessage;

bool decode_activation_message(const char* json_str, ActivationMessage* out_msg);
bool decode_status_change_message(const char* json_str, StatusChangeMessage* out_msg);
bool decode_update_record_request_message(const char* json_str, UpdateRecordRequestMessage* out_msg);
bool decode_feedback_message(const char* json_str, FeedbackMessage* out_msg);
bool decode_records_feedback_message(const char* json_str, RecordsFeedbackMessage* out_msg);
#endif