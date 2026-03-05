#include "records_info_message_creator.h"

#include "message_queue_manager.h"
#include "timer_state_info_message_creator.h"
#include "matching_message_encoder.h"

#include "../ble/include/ble_controller.h"
#include "../ble/include/ble_internal.h"

#include "../storage/log_types.h"
#include "../storage/therapy_counter.h"
#include "../storage/log_reader.h"
#include "../storage/log_storage.h"
#include "../storage/profile_partition_manager.h"

#include "../helper/binary_message_parser.h"

#include "../manager/session_timer_getter.h"
#include "../manager/current_therapy_state_manager.h"
#include "../manager/therapy_id_manager.h"
#include "../manager/therapy_duration_manager.h"
#include "../manager/timer_info_getter.h"
#include "../manager/profile_manager.h"

#include "../device_configuration.h"

#include "esp_err.h"
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "RecordsInfoMessageCreator";
static SemaphoreHandle_t s_records_mutex;

static uint8_t s_records_slot_buf[THERAPY_SLOT_SIZE];

// Slotu okuyup logları fragment formatına hazırlar.
static bool set_records(uint16_t therapy_id, LogReadMode log_read_mode) {
    ESP_LOGI(TAG, "Set records for therapy_id: %u", therapy_id);

    uint32_t base_offset = log_reader_therapy_id_to_base_offset(therapy_id);

    log_storage_lock();
    esp_err_t e = log_storage_read_slot(base_offset, s_records_slot_buf, sizeof(s_records_slot_buf));
    log_storage_unlock();
    if (e != ESP_OK) return false;

    ReadTherapyLogs read_therapy_logs = {0};

    if(log_reader_read_records(therapy_id, &read_therapy_logs, log_read_mode, s_records_slot_buf)) {
        init_fragments();

        ReadTherapyInfo therapy_info;

        if(log_reader_read_therapy_info(therapy_id, &therapy_info, true, s_records_slot_buf)) {
            if(therapy_id == get_current_therapy_id()) {
                ESP_LOGI(TAG, "current start_encoding_for_new_therapy of therapy id: %u",therapy_id);
                uint16_t therapy_passed_seconds = (uint16_t)((get_paused_therapy_passed_duration_ms() + get_therapy_timer_passed_ms()) /1000u);
                start_encoding_for_new_therapy(therapy_id, get_planned_therapy_duration_s(), therapy_passed_seconds);
            }

            else {
                ESP_LOGI(TAG, "other start_encoding_for_new_therapy of therapy id: %u",therapy_id);
                start_encoding_for_new_therapy(therapy_id, therapy_info.therapy_duration, therapy_info.passed_duration);
            }

            if (read_therapy_logs.count_measurements > 0) {
                ESP_LOGI(TAG, "count_measurements: %u", read_therapy_logs.count_measurements);
                encode_records_of_therapy(therapy_id, 0x03, 4, read_therapy_logs.count_measurements, read_therapy_logs.measurements);
            }
            if (read_therapy_logs.count_notifications > 0) {
                ESP_LOGI(TAG, "count_notifications: %u", read_therapy_logs.count_notifications);
                encode_records_of_therapy(therapy_id, 0x04, 3, read_therapy_logs.count_notifications, read_therapy_logs.notifications);
            }
            if (read_therapy_logs.count_brightness > 0) {
                ESP_LOGI(TAG, "count_brightness: %u", read_therapy_logs.count_brightness);
                encode_records_of_therapy(therapy_id, 0x05, 8, read_therapy_logs.count_brightness, read_therapy_logs.brightness_updates);
            }

            if(!add_fragment_count()){
                log_reader_free_therapy_logs(&read_therapy_logs);
                return false;
            }
        }
        else {
            ESP_LOGE(TAG, "Failed to read therapy info for therapy id: %u", therapy_id);
            log_reader_free_therapy_logs(&read_therapy_logs);
            return false;
        }

        log_reader_free_therapy_logs(&read_therapy_logs);
        return true;
    }
    
    ESP_LOGE(TAG, "Failed to read therapy records for therapy id: %u", therapy_id);
    log_reader_free_therapy_logs(&read_therapy_logs);
    return false;
}

// Üretilen fragment'ları düşük öncelikli kuyruğa gönderir.
static void send_fragments(uint16_t therapy_id) {
    uint16_t fragment_count = get_fragment_count();
    ESP_LOGI(TAG, "therapy_id: %d, fragment_count: %d", therapy_id, fragment_count);

    if (fragment_count == 0) {
        ESP_LOGW(TAG, "No fragments generated for therapy_id: %u", therapy_id);
        return;
    }

    for (uint16_t i = 0; i < fragment_count; i++) {
        const uint8_t* frag = get_fragment(i);
        size_t len = get_fragment_length(i);
        ESP_LOGI(TAG, "---------------------------------------");
        ESP_LOGI(TAG, "fragment id: %d, length: %d", i, len);


        if (!frag || len == 0) continue;

        if (!wait_low_queue_space(2, 2000)) {
            ESP_LOGE(TAG, "Low queue is congested; abort sending remaining fragments for therapy=%u", therapy_id);
            return;
        }

        ESP_LOGI(TAG, "Sending fragment for therapy_id: %d, fragment_index: %d", therapy_id, i);
        ESP_LOG_BUFFER_HEX(TAG, frag, len);
        send_records_info_message_to_queue(therapy_id, (uint8_t*)frag, len);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Belirli terapi için records hazırlayıp gönderir.
static void send_records_info_message(uint16_t therapy_id, bool is_active_therapy) {
    if (therapy_id == 0) {
        ESP_LOGW(TAG, "send_records_info_message called with therapy_id=0, ignoring.");
        return;
    }

    if (s_records_mutex) {
        if (xSemaphoreTake(s_records_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGE(TAG, "Records mutex timeout, skipping therapy_id=%u", therapy_id);
            return;
        }
    }

    LogReadMode log_read_mode = is_active_therapy ? LOG_READ_UNTIL_LAST_BLE_CONNECTED : LOG_READ_FULL_SLOT;
    bool is_records_set = set_records(therapy_id, log_read_mode);
    if (is_records_set) {
        send_fragments(therapy_id);
    } else {
        ESP_LOGE(TAG, "Unable to prepare records for therapy id: %u", therapy_id);
    }

    if (s_records_mutex){
        xSemaphoreGive(s_records_mutex);
    }
}

// Uygulamanın ACK/NACK geri bildirimi.
static void on_record_request(const uint8_t *buf, size_t len){
    RecordRequestMessage record_request_message;

    if(!decode_record_request_message_bin(buf, &record_request_message)) {
        return;
    }

    if (record_request_message.therapy_id == 0) {
        ESP_LOGW(TAG, "Cannot send records for therapy id 0");
        return;
    }
    bool is_active_therapy = record_request_message.therapy_id == get_current_therapy_id();
    send_records_info_message(record_request_message.therapy_id, is_active_therapy);
}


// Records aktarım akışını başlatır ve callback'leri bağlar.
void init_records_info_message_creator() {
    s_records_mutex = xSemaphoreCreateMutex();

    register_on_write_record_request_callback(on_record_request);
}