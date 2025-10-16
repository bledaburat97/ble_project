#include <stdint.h>
#include "message_queue_manager.h"
#include "ble/include/ble_controller.h"
#include "ble/include/ble_connection_state_manager.h"
#include "device_configuration.h"
#include "ble/include/ble_internal.h"

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


static const char *TAG = "MessageQueueManager";
static void (*timer_state_info_feedback_callback)() = NULL;
static void (*record_pending_approval_timeout_callback)(uint16_t) = NULL;
static void (*send_record_again_callback)(uint16_t) = NULL;
static void (*send_new_record_callback)(uint16_t) = NULL;

static int dynamic_period = DYNAMIC_PERIOD;

QueueHandle_t high_priority_queue;
QueueHandle_t low_priority_queue;
static RecordsPending pending_record = {0};

static void (*device_info_listeners[MAX_STATE_LISTENERS])();
static int device_info_listener_count = 0;

static inline bool requires_conf(MessageType t) {
    switch (t) {
        case TIMER_STATE_INFO_MESSAGE:
        case NOTIFICATION_INFO_MESSAGE:
        case DEVICE_INFO_MESSAGE:
            return true;   // bunları indicate yapıyorsun
        case MEASUREMENT_INFO_MESSAGE:
        case RECORDS_INFO_MESSAGE:
        default:
            return false;  // notify → CONF yok
    }
}

static void check_pending_timeouts()
{
    uint32_t now = esp_log_timestamp();
    if(pending_record.active) {
        if (now - pending_record.send_timestamp > MAX_TIMEOUT_DURATION) {
            ESP_LOGE(TAG, "Records timeout (therapy id=%d)", pending_record.therapy_id);
            pending_record.active = false;
            if (pending_record.retry_count < MAX_MESSAGE_RETRY_COUNT) {
                if (send_record_again_callback) {
                    send_record_again_callback(pending_record.therapy_id);
                }
            }
            else{
                if (send_new_record_callback) {
                    send_new_record_callback(pending_record.therapy_id);
                }
            }
        }
    }
}

static void set_record_pending(uint16_t therapy_id) {
    if(pending_record.active) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    pending_record.active = true;
    if(pending_record.therapy_id == therapy_id) {
        pending_record.retry_count += 1;
    }
    else {
        pending_record.therapy_id = therapy_id;
        pending_record.retry_count = 1;
    }
    pending_record.send_timestamp = esp_log_timestamp();
}

static inline void drain_old_conf(void){
    esp_gatt_status_t dummy;
    // ble_wait_for_indication_conf zaten sem’e bağlı ise:
    (void)ble_wait_for_indication_conf(&dummy, 0 /*ms*/);
}

static void send_and_track(const MessageQueueEntry *entry)
{
    if (!get_ble_connection_status()) {
        ESP_LOGW(TAG, "No active BLE connection, cannot send message.");
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    SemaphoreHandle_t ble_mutex = get_ble_mutex_handle();
    const uint32_t conf_timeout_ms = 3000;
    const bool need_conf = requires_conf(entry->type);

    if (need_conf) {
        if(entry->type == NOTIFICATION_INFO_MESSAGE && !notif_ind_enabled) {
            ESP_LOGW(TAG, "Indication needed (type=%d) but CCCD not enabled.", entry->type);
            return;
        }

        else if(entry->type == DEVICE_INFO_MESSAGE && !device_ind_enabled) {
            ESP_LOGW(TAG, "Indication needed (type=%d) but CCCD not enabled.", entry->type);
            return;
        }
        else if(entry->type == TIMER_STATE_INFO_MESSAGE && !timer_ind_enabled) {
            ESP_LOGW(TAG, "Indication needed (type=%d) but CCCD not enabled.", entry->type);
            return;
        }

        drain_old_conf();
    }

    for (int attempt = 1; attempt <= MAX_MESSAGE_RETRY_COUNT; attempt++) {
        if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(300)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_DURATION));
            continue;
        }

        esp_err_t ret = ble_send_info_message_with_type(entry->type, entry->data, entry->data_length);
        xSemaphoreGive(ble_mutex);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Send fail (type=%d, len=%u), attempt %d/%d: %s",
                     entry->type, (unsigned)entry->data_length, attempt, MAX_MESSAGE_RETRY_COUNT, esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_DURATION));
            continue;
        }

        if (!need_conf) {
            if (entry->wait_for_response && entry->type == RECORDS_INFO_MESSAGE) {
                //add_pending_records(entry->id);  // son fragment ise app feedback bekle
                set_record_pending(entry->id);
            }
            return;  // sıradaki mesaja geç
        }

        // Indicate: CONF bekle (mevcut mantık)
        esp_gatt_status_t st = ESP_GATT_ERROR;
        bool got_conf = ble_wait_for_indication_conf(&st, conf_timeout_ms);
        if (got_conf && st == ESP_GATT_OK) {

            ESP_LOGI(TAG, "CONF OK for type=%d", entry->type);

            if(entry->type == DEVICE_INFO_MESSAGE) {
                for (int i = 0; i < device_info_listener_count; i++) {
                    if (device_info_listeners[i]) {
                        device_info_listeners[i]();
                    }
                }
            }

            else if(entry->type == TIMER_STATE_INFO_MESSAGE) {
                if (timer_state_info_feedback_callback) {
                    timer_state_info_feedback_callback();
                }
            }

            return; 
        }

        ESP_LOGW(TAG, "CONF %s (status=%d) attempt %d/%d",
                 got_conf ? "NOK" : "TIMEOUT", (int)st, attempt, MAX_MESSAGE_RETRY_COUNT);

        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_DURATION));
    }

    ESP_LOGE(TAG, "All attempts failed for type=%d", entry->type);
}

static void on_dynamic_period_change(uint16_t period) {
    ESP_LOGI(TAG, "Dynamic period is changed: %u", period);
    dynamic_period = (int)period;
}

static void queue_sender_task(void *pvParameters)
{
    MessageQueueEntry entry;

    while (1) {

        if (!get_ble_connection_status() || !bond_ok) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        TickType_t inter_message_delay = pdMS_TO_TICKS(dynamic_period);

        if (xQueueReceive(high_priority_queue, &entry, pdMS_TO_TICKS(100)) == pdTRUE) {
            //ESP_LOGI(TAG, "Received high priority message in the queue.");
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
        vTaskDelay(1);
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
        .wait_for_response = wait_for_response // yalnızca son fragment true
    };
    
    if(xQueueSend(low_priority_queue, &entry, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send message to queue");
        free(data_copy);
    }
} 

void send_info_message_to_queue(MessageType message_type, uint8_t* data, size_t data_length) {
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
        .id = 0,
        .wait_for_response = false
    };
    
    TickType_t time_out = 0; // asla block etme
    if(xQueueSend(high_priority_queue, &entry, time_out) != pdTRUE) {
        ESP_LOGW(TAG, "Queue full, dropping message type=%u", (unsigned)message_type);
        free(data_copy);
    }
}

bool clear_pending_approval_record(uint16_t therapy_id)
{
    if(!pending_record.active || pending_record.therapy_id != therapy_id) {
        ESP_LOGE(TAG, "Big error.");
        return false;
    }
    pending_record.active = false;
    return true;
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

void register_send_record_again_callback(void (*callback)(uint16_t)) {
    send_record_again_callback = callback;
}

void register_send_new_record_callback(void (*callback)(uint16_t)) {
    send_new_record_callback = callback;
}

bool is_record_pending(void) {
    return pending_record.active;
}