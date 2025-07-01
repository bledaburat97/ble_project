#include <string.h>
#include <stdint.h>
#include "ble_control.h"

#ifndef MESSAGE_QUEUE_MANAGER_H
#define MESSAGE_QUEUE_MANAGER_H

typedef struct {
    MessageType type;           // Mesaj tipi
    uint8_t *data;              // Data (JSON string)
    size_t data_length;         // Data uzunluğu
    uint16_t message_id;        // Mesaj ID (Aynı type için ayırt edici ID) -- öneririm
} MessageQueueEntry;

typedef struct {
    MessageType type;           // Mesaj tipi
    uint8_t *data;              // Data (JSON string)
    size_t data_length;         // Data uzunluğu
    uint32_t send_timestamp;    // Gönderildiği zaman (esp_timer_get_time / esp_log_timestamp)
    uint16_t message_id;        // Mesaj ID (Aynı type için ayırt edici ID) -- öneririm
} PendingMessage;

void send_info_message(MessageType message_type, uint8_t* data, size_t data_length, uint16_t message_id);
void init_message_queue_manager();
void register_device_info_feedback_callback(void (*callback)());
void register_timer_state_info_feedback_callback(void (*callback)());
void process_feedback_message(uint16_t ack_message_id);

#endif