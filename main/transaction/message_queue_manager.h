#ifndef MESSAGE_QUEUE_MANAGER_H
#define MESSAGE_QUEUE_MANAGER_H

#include <string.h>
#include <stdint.h>
#include "ble/include/ble_controller.h"
#include <stdbool.h>

typedef struct {
    MessageType type;           // Mesaj tipi
    uint8_t *data;              // Data
    size_t data_length;         // Data uzunluğu
    uint16_t id;                // Records için therapy_id, diğerlerinde 0
    uint32_t enq_ts;
} MessageQueueEntry;

void send_info_message_to_queue(MessageType message_type, uint8_t* data, size_t data_length);
void send_records_info_message_to_queue(uint16_t therapy_id, uint8_t* data, size_t data_length);
void init_message_queue_manager();
void register_device_info_feedback_callback(void (*callback)());
void register_timer_state_info_feedback_callback(void (*callback)());
bool wait_low_queue_space(uint32_t min_free, uint32_t timeout_ms);

#endif