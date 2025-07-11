#include <string.h>
#include <stdint.h>
#include "ble_control.h"

#ifndef MESSAGE_QUEUE_MANAGER_H
#define MESSAGE_QUEUE_MANAGER_H

#define MAX_STATE_LISTENERS 5

typedef struct {
    MessageType type;           // Mesaj tipi
    uint8_t *data;              // Data (JSON string)
    size_t data_length;         // Data uzunluğu
    uint16_t id;                // Mesaj ID veya Terapi ID (Aynı type için ayırt edici ID) -- öneririm
    bool wait_for_response;
} MessageQueueEntry;

typedef struct {
    MessageType type;           // Mesaj tipi
    uint32_t send_timestamp;    // Gönderildiği zaman (esp_timer_get_time / esp_log_timestamp)
    uint16_t message_id;        // Mesaj ID (Aynı type için ayırt edici ID) -- öneririm
} PendingApprovalMessage;

typedef struct {
    uint16_t therapy_id;
    uint32_t send_timestamp;    // Gönderildiği zaman (esp_timer_get_time / esp_log_timestamp)
} PendingApprovalRecords;

void send_info_message_to_queue(MessageType message_type, uint8_t* data, size_t data_length, uint16_t message_id);
void send_records_info_message_to_queue(uint16_t therapy_id, uint8_t* data, size_t data_length, bool wait_for_response);
void init_message_queue_manager();
void register_device_info_feedback_callback(void (*callback)());
void register_timer_state_info_feedback_callback(void (*callback)());
void process_feedback_message(uint16_t ack_message_id);
bool clear_pending_approval_record(uint16_t therapy_id);
void register_on_record_pending_approval_timeout_callback(void (*callback)(uint16_t));

#endif