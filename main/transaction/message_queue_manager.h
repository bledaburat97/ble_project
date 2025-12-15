#ifndef MESSAGE_QUEUE_MANAGER_H
#define MESSAGE_QUEUE_MANAGER_H

#include <string.h>
#include <stdint.h>
#include "ble/include/ble_controller.h"
#include <stdbool.h>

typedef struct {
    MessageType type;           // Mesaj tipi
    uint8_t *data;              // Data (JSON string)
    size_t data_length;         // Data uzunluğu
    uint16_t id;                // Records için therapy_id, diğerlerinde 0
    bool wait_for_response;
} MessageQueueEntry;

typedef struct {
    bool     active;        // şu anda bir terapi record’u beklemede mi?
    uint16_t therapy_id;    // hangi terapi
    uint8_t  retry_count;   // son batch için kaç kez yeniden denedik (0..2)
    uint32_t send_timestamp;  // son "son fragment" gönderim zaman damgası
} RecordsPending;

void send_info_message_to_queue(MessageType message_type, uint8_t* data, size_t data_length);
void send_records_info_message_to_queue(uint16_t therapy_id, uint8_t* data, size_t data_length, bool wait_for_response);
void init_message_queue_manager();
void register_device_info_feedback_callback(void (*callback)());
void register_timer_state_info_feedback_callback(void (*callback)());
bool clear_pending_approval_record(uint16_t therapy_id);
void register_send_record_again_callback(void (*callback)(uint16_t));
void register_send_new_record_callback(void (*callback)(uint16_t));
bool is_record_pending(void);

#endif