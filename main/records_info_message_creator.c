#include "records_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "ble/include/ble_controller.h"
#include "ble/include/ble_internal.h"
#include "message_queue_manager.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "timer_state_info_message_creator.h"
#include "storage/log_writer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "binary_message_parser.h"
#include "state_manager.h"
#include "current_therapy_info_manager.h"
#include "timer_management.h"
#include "device_configuration.h"

static const char *TAG = "RecordsInfoMessageCreator";
static uint16_t final_therapy_id_to_be_sent;

static void set_records(uint16_t therapy_id) {
    ESP_LOGI(TAG, "Set records");
    if(therapy_id == 0) {
        ESP_LOGE(TAG, "There should be a saved therapy.");
        return;
    }
    ReadTherapyLogs read_therapy_logs;
    bool is_active_therapy = (therapy_id == read_therapy_count()) && (get_current_therapy_state() == ACTIVE || get_current_therapy_state() == PAUSED);

    if(read_records(therapy_id, &read_therapy_logs, is_active_therapy)) {
        init_fragments();

        ReadTherapyInfo therapy_info;
        if(read_therapy_info(therapy_id, &therapy_info)) {
            if(therapy_id == get_current_therapy_id()) {
                start_encoding_for_new_therapy(therapy_id, get_current_therapy_duration(), get_session_passed_seconds());
            }

            else {
                start_encoding_for_new_therapy(therapy_id, therapy_info.therapy_duration, therapy_info.passed_duration);
            }

            if (read_therapy_logs.count_measurements > 0) {
                //ESP_LOGI(TAG, "count_measurements: %u", read_therapy_logs.count_measurements);
                encode_records_of_therapy(therapy_id, 0x03, 4, read_therapy_logs.count_measurements, read_therapy_logs.measurements);
            }
            if (read_therapy_logs.count_notifications > 0) {
                //ESP_LOGI(TAG, "count_notifications: %u", read_therapy_logs.count_notifications);
                encode_records_of_therapy(therapy_id, 0x04, 3, read_therapy_logs.count_notifications, read_therapy_logs.notifications);
            }
            if (read_therapy_logs.count_brightness > 0) {
                //ESP_LOGI(TAG, "count_brightness: %u", read_therapy_logs.count_brightness);
                encode_records_of_therapy(therapy_id, 0x05, 8, read_therapy_logs.count_brightness, read_therapy_logs.brightness_updates);
            }

            add_fragment_count();
        }

        free(read_therapy_logs.measurements);
        free(read_therapy_logs.notifications);
        free(read_therapy_logs.brightness_updates);
    }
}

static void send_fragments(uint16_t therapy_id) {
    uint16_t fragment_count = get_fragment_count();
    ESP_LOGI(TAG, "therapy_id: %d, fragment_count: %d", therapy_id, fragment_count);

    for (uint16_t i = 0; i < fragment_count; i++) {
        const uint8_t* frag = get_fragment(i);
        size_t len = get_fragment_length(i);
        ESP_LOGI(TAG, "---------------------------------------");
        ESP_LOGI(TAG, "fragment id: %d, length: %d", i, len);

        if (frag != NULL && len > 0) {
            ESP_LOGI(TAG, "Sending fragment for therapy_id: %d, fragment_index: %d", therapy_id, i);
            ESP_LOG_BUFFER_HEX(TAG, frag, len);
            bool is_last = (i == fragment_count - 1);
            send_records_info_message_to_queue(therapy_id, (uint8_t*)frag, len, is_last);
            vTaskDelay(pdMS_TO_TICKS(10));  //TODO Gerekirse bu süre MTU'ya göre ayarlanabilir
        }
    }
}

void send_records_info_message(uint16_t therapy_id) {
    set_records(therapy_id);
    send_fragments(therapy_id);
}

static void on_active_or_paused_therapy_existed()
{
    if(is_record_pending()) {
        ESP_LOGE(TAG, "is_record_pending true");
        return;
    }
    send_records_info_message(get_current_therapy_id());
}

void on_write_of_record_request_message(const uint8_t *buf, size_t len) {
    if(is_record_pending()) {
        ESP_LOGI(TAG, "is_record_pending true");
        return;
    }
    ESP_LOGI(TAG, "on_write_of_record_request_message");

    UpdateRecordRequestMessage record_request_message;
    if(!decode_update_record_request_message_bin(buf, &record_request_message)) {
        return;
    }

    uint16_t last_therapy_id_saved_in_app = record_request_message.last_therapy_id;

    uint16_t last_saved_therapy_id = read_therapy_count();
    ESP_LOGI(TAG, "last saved therapy_id: %d", last_saved_therapy_id);

    //active therapy'nin bilgilerini zaten aktif terapi bilgi mesajında göndermiş olmalıyız.
    if (get_current_therapy_state() == ACTIVE || get_current_therapy_state() == PAUSED) {
        final_therapy_id_to_be_sent = (last_saved_therapy_id > 0) ? (last_saved_therapy_id - 1) : 0;
    } else {
        final_therapy_id_to_be_sent = last_saved_therapy_id;
    }

    if (final_therapy_id_to_be_sent == 0) {
        ESP_LOGI(TAG, "No finished therapy to send");
        return;
    }

    uint16_t window_start = 1;
    if (final_therapy_id_to_be_sent >= MAX_RECORDS_TO_BE_SENT) {
        window_start = final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT + 1;
    }

    // gerçek başlangıç (app’in last_saved+1’inden geriye düşme)
    uint16_t start_id = last_therapy_id_saved_in_app + 1;
    if (start_id < window_start) {
        start_id = window_start;
    }

    if (start_id > final_therapy_id_to_be_sent) {
        ESP_LOGI(TAG, "Nothing to send in window (start_id=%u, final=%u)", start_id, final_therapy_id_to_be_sent);
        return;
    }
}

static void on_records_feedback(const uint8_t *buf, size_t len){
    RecordsFeedbackMessage records_feedback_message;

    if(!decode_records_feedback_message_bin(buf, &records_feedback_message)) {
        return;
    }
    if (!clear_pending_approval_record(records_feedback_message.therapy_id)) {
        ESP_LOGE(TAG, "There is not any pending approval record of this therapy id: %u", records_feedback_message.therapy_id);
    }
    if(records_feedback_message.is_success) {
        uint16_t therapy_id = records_feedback_message.therapy_id + 1;
        if(therapy_id > final_therapy_id_to_be_sent) {
            ESP_LOGE(TAG, "There is not therapy record to be sent");
            return;
        }
        else {
            send_records_info_message(therapy_id);
        }
    }
    else {
        send_records_info_message(records_feedback_message.therapy_id);
    }
}

static void send_record_again(const uint16_t therapy_id) {
    send_records_info_message(therapy_id);
}

static void send_new_record(const uint16_t therapy_id) {
    if(therapy_id == get_current_therapy_id()){
        return;
    }

    uint16_t new_therapy_id = therapy_id + 1;
    if(new_therapy_id > final_therapy_id_to_be_sent) {
        ESP_LOGI(TAG, "There is not therapy record to be sent");
        return;
    }
    else {
        send_records_info_message(new_therapy_id);
    }
}

void init_records_info_message_creator() {
    register_active_or_paused_therapy_info(on_active_or_paused_therapy_existed);
    register_on_write_records_feedback_callback(on_records_feedback);
    register_on_write_updating_records_callback(on_write_of_record_request_message);
    register_send_record_again_callback(send_record_again);
    register_send_new_record_callback(send_new_record);

    if(get_current_therapy_state() == NONE) {
        final_therapy_id_to_be_sent = read_therapy_count();
    }
    else {
        final_therapy_id_to_be_sent = read_therapy_count() - 1;
    }
    ESP_LOGI(TAG, "final_therapy_id_to_be_sent: %u", final_therapy_id_to_be_sent);
}

