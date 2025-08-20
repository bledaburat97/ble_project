#include <stdint.h>
#include "message_queue_manager.h"
#include "ble_control.h"
#include "ble/ble_state_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define MAX_PENDING_MESSAGES 100
#define MAX_MESSAGE_DATA_SIZE 500

static const char *TAG = "MessageQueueManager";
static uint16_t MAX_TIMEOUT_DURATION = 50000;
static uint16_t dynamic_period = 1000;
static void (*device_info_feedback_callback)() = NULL;
static void (*timer_state_info_feedback_callback)() = NULL;
static void (*record_pending_approval_timeout_callback)(uint16_t) = NULL;

QueueHandle_t high_priority_queue;
QueueHandle_t low_priority_queue;
PendingApprovalMessage pending_approval_messages[MAX_PENDING_MESSAGES];
PendingApprovalRecords pending_approval_records[MAX_PENDING_MESSAGES];

static void (*device_info_listeners[MAX_STATE_LISTENERS])();
static int device_info_listener_count = 0;

static void clear_pending_message(int index)
{
    pending_approval_messages[index].type = 0;
}

static void clear_pending_record(int index)
{
    pending_approval_records[index].therapy_id = 0;
}

static void check_pending_timeouts()
{
    uint32_t now = esp_log_timestamp();

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_messages[i].type != 0) {
            if (now - pending_approval_messages[i].send_timestamp > MAX_TIMEOUT_DURATION) {
                ESP_LOGE(TAG, "Message timeout (type=%d id=%u)", pending_approval_messages[i].type, pending_approval_messages[i].message_id);
                clear_pending_message(i);
            }
        }
    }

    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_records[i].therapy_id > 0) {
            if (now - pending_approval_records[i].send_timestamp > MAX_TIMEOUT_DURATION) {
                ESP_LOGE(TAG, "Records timeout (therapy id=%d)", pending_approval_records[i].therapy_id);
                record_pending_approval_timeout_callback(pending_approval_records[i].therapy_id);
                clear_pending_record(i);
            }
        }
    }
}

static void add_pending_message(const MessageQueueEntry *entry)
{
    ESP_LOGI(TAG, "Add pending message.");
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_messages[i].type == 0) { // boş slot
            pending_approval_messages[i].type = entry->type;
            pending_approval_messages[i].send_timestamp = esp_log_timestamp();
            pending_approval_messages[i].message_id = entry->id;
            break;
        }
    }
}

static void add_pending_records(const uint16_t therapy_id) {
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_records[i].therapy_id == 0) { //boş slot
            pending_approval_records[i].therapy_id = therapy_id;
            pending_approval_records[i].send_timestamp = esp_log_timestamp();
            break;
        }
    }
}

static void send_and_track(const MessageQueueEntry *entry)
{
    if (!get_ble_connection_status()) {
        ESP_LOGW(TAG, "No active BLE connection, cannot send message.");
        return;
    }

    int retry_count = 0;
    const int max_retries = 2;
    const int retry_delay_ms = 100;
    SemaphoreHandle_t ble_mutex = get_ble_mutex_handle();

    while (retry_count < max_retries) {
        if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(150)) == pdTRUE) {
            esp_err_t ret = ble_send_info_message_with_type(entry->type, entry->data, entry->data_length);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Message sent (type=%d, id=%u, length=%u)", entry->type, entry->id, entry->data_length);
                ESP_LOGI(TAG, "---------------------------------------");

                if (entry->wait_for_response) {
                    if(entry->type == RECORDS_INFO_MESSAGE) {
                        add_pending_records(entry->id);
                    }
                    else{
                        add_pending_message(entry);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Failed to send message: %s", esp_err_to_name(ret));
            }

            xSemaphoreGive(ble_mutex);
            return;
        } else {
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
        }
    }
}

static void on_dynamic_period_change(uint16_t period) {
    ESP_LOGI(TAG, "Dynamic period is changed: %u", period);
    dynamic_period = period;
}

static void queue_sender_task(void *pvParameters)
{
    MessageQueueEntry entry;


    while (1) {
        TickType_t inter_message_delay = pdMS_TO_TICKS(dynamic_period);

        if (xQueueReceive(high_priority_queue, &entry, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Received high priority message in the queue.");
            send_and_track(&entry);
            free(entry.data);
            
            vTaskDelay(inter_message_delay);
            continue;
        }

        if (xQueueReceive(low_priority_queue, &entry, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Received low priority message in the queue.");
            send_and_track(&entry);
            free(entry.data);
            
            vTaskDelay(inter_message_delay);
            continue;
        }

        check_pending_timeouts();
    }
}

void init_message_queue_manager()
{
    high_priority_queue = xQueueCreate(MAX_PENDING_MESSAGES, sizeof(MessageQueueEntry));
    low_priority_queue = xQueueCreate(MAX_PENDING_MESSAGES, sizeof(MessageQueueEntry));

    if (high_priority_queue == NULL || low_priority_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queues!");
    }
    register_dynamic_period_change_callback(on_dynamic_period_change);
    xTaskCreatePinnedToCore(queue_sender_task, "queue_sender", 4096, NULL, 5, NULL, tskNO_AFFINITY);

}


void send_records_info_message_to_queue(uint16_t therapy_id, uint8_t* data, size_t data_length, bool wait_for_response) {
    ESP_LOGI(TAG, "Adding records message of %u to queue to send it", therapy_id);

    uint8_t* data_copy = (uint8_t*)malloc(data_length);
    if (data_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for message copy");
        return;
    }

    memcpy(data_copy, data, data_length);

    MessageQueueEntry entry = {
        .type = RECORDS_INFO_MESSAGE,
        .data = data_copy,
        .data_length = data_length,
        .id = therapy_id,
        .wait_for_response = wait_for_response
    };
    
    if(xQueueSend(low_priority_queue, &entry, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send message to queue");
        free(data_copy);
    }
} 

void send_info_message_to_queue(MessageType message_type, uint8_t* data, size_t data_length, uint16_t message_id) {
    ESP_LOGI(TAG, "Adding message of %u to queue to send it", message_type);
    
    uint8_t* data_copy = (uint8_t*)malloc(data_length);
    if (data_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for message copy");
        return;
    }
    memcpy(data_copy, data, data_length);

    MessageQueueEntry entry = {
        .type = message_type,
        .data = data_copy,
        .data_length = data_length,
        .id = message_id,
        .wait_for_response = true
    };
    
    if(xQueueSend(high_priority_queue, &entry, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send message to queue");
        free(data_copy);
    }
}

void process_feedback_message(uint16_t ack_message_id)
{
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_messages[i].message_id == ack_message_id) {
            
            ESP_LOGW(TAG, "Feedback received for message (type=%d id=%u)", pending_approval_messages[i].type, ack_message_id);

            if(pending_approval_messages[i].type == DEVICE_INFO_MESSAGE) {
                for (int i = 0; i < device_info_listener_count; i++) {
                    if (device_info_listeners[i]) {
                        device_info_listeners[i]();
                    }
                }
            }

            else if(pending_approval_messages[i].type == TIMER_STATE_INFO_MESSAGE) {
                if (timer_state_info_feedback_callback) {
                    timer_state_info_feedback_callback();
                }
            }

            clear_pending_message(i);
            break;
        }
    }
}

bool clear_pending_approval_record(uint16_t therapy_id)
{
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_approval_records[i].therapy_id == therapy_id) {
            clear_pending_record(i);
            return true;
        }
    }
    return false;
}

void register_device_info_feedback_callback(void (*callback)()) {
    if (device_info_listener_count < MAX_STATE_LISTENERS) {
        device_info_listeners[device_info_listener_count++] = callback;
        ESP_LOGI(TAG, "Registered device info listener (%d total)", device_info_listener_count);
    } else {
        ESP_LOGW(TAG, "Max device info listeners reached.");
    }
}

void register_timer_state_info_feedback_callback(void (*callback)()) {
    timer_state_info_feedback_callback = callback;
}

void register_on_record_pending_approval_timeout_callback(void (*callback)(uint16_t)) {
    record_pending_approval_timeout_callback = callback;
}