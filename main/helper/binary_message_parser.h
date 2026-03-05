
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef BINARY_MESSAGE_PARSER_H
#define BINARY_MESSAGE_PARSER_H

typedef enum {
    PAUSE = 0x01,
    STOP = 0x02,
    CONTINUE = 0x03
} StatusChange;

typedef struct {
    uint16_t duration;
    uint8_t brightness[6];
} ActivationMessage;

typedef struct {
    StatusChange type;
} StatusChangeMessage;

typedef struct {
    uint16_t last_therapy_id;
    uint32_t profile_id;
} ProfileInfoMessage;

typedef struct {
    uint16_t therapy_id;
} RecordRequestMessage;

typedef struct {
    uint32_t passkey;
} UpdatePasskeyMessage;

bool decode_activation_message_bin(const uint8_t *buf, ActivationMessage *out_msg);
bool decode_status_change_message_bin(const uint8_t *buf, StatusChangeMessage *out_msg);
bool decode_profile_info_message_bin(const uint8_t *buf, size_t len, ProfileInfoMessage *out_msg);
bool decode_record_request_message_bin(const uint8_t *buf, RecordRequestMessage *out_msg);
bool decode_update_passkey_message_bin(const uint8_t *buf, size_t len, UpdatePasskeyMessage *out_msg);
#endif