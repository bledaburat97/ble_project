#include "records_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "ble_control.h"
#include "message_queue_manager.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "timer_state_info_message_creator.h"
#include "storage/log_writer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "json_parser.h"
#include "state_manager.h"
#include "current_therapy_info_manager.h"

static const char *TAG = "RecordsInfoMessageCreator";
static uint8_t MAX_RECORDS_TO_BE_SENT = 20;
static uint16_t final_therapy_id_to_be_sent;

void send_records_info_message(uint16_t therapy_id) {
    set_records(therapy_id);
    uint16_t fragment_count = get_fragment_count();
    ESP_LOGI(TAG, "therapy_id: %d, fragment_count: %d", therapy_id, fragment_count);

    for (uint16_t i = 0; i <= fragment_count; i++) {
        const uint8_t* frag = get_fragment(i);
        size_t len = get_fragment_length(i);
        ESP_LOGI(TAG, "---------------------------------------");
        ESP_LOGI(TAG, "fragment id: %d, length: %d", i, len);

        if (frag != NULL && len > 0) {
            ESP_LOGI(TAG, "Sending fragment for therapy_id: %d, fragment_index: %d", therapy_id, i);
            ESP_LOG_BUFFER_HEX(TAG, frag, len);
            send_records_info_message_to_queue(therapy_id, (uint8_t*)frag, len,  i == fragment_count);
            vTaskDelay(pdMS_TO_TICKS(10));  //TODO Gerekirse bu süre MTU'ya göre ayarlanabilir
        }
    }
}

static void on_active_or_paused_therapy_existed()
{
    send_records_info_message(read_therapy_count());
}

void on_write_of_record_request_message(const char *data) {
    ESP_LOGI(TAG, "on_write_of_record_request_message");

    UpdateRecordRequestMessage record_request_message;
    if(!decode_update_record_request_message(data, &record_request_message)) {
        return;
    }

    uint16_t last_therapy_id_saved_in_app = record_request_message.last_therapy_id;

    uint16_t last_saved_therapy_id = read_therapy_count();
    ESP_LOGI(TAG, "last saved therapy_id: %d", last_saved_therapy_id);

    //active therapy'nin bilgilerini zaten aktif terapi bilgi mesajında göndermiş olmalıyız.
    final_therapy_id_to_be_sent = get_device_state() == STATE_ACTIVE ? (last_saved_therapy_id - 1) : last_saved_therapy_id;

    uint16_t therapy_id = (last_therapy_id_saved_in_app >= (final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT) || final_therapy_id_to_be_sent < MAX_RECORDS_TO_BE_SENT)  ? last_therapy_id_saved_in_app + 1 : final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT + 1;
    if (therapy_id > final_therapy_id_to_be_sent) {
        ESP_LOGE(TAG, "There is not therapy record to be sent");
        return;
    }
    send_records_info_message(therapy_id);
}

static void on_records_feedback(const char *data){
    RecordsFeedbackMessage records_feedback_message;

    if(!decode_records_feedback_message(data, &records_feedback_message)) {
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
    clear_pending_approval_record(records_feedback_message.therapy_id);
}

static void on_record_pending_approval_timeout(const uint16_t therapy_id) {
    send_records_info_message(therapy_id);
}

void init_records_info_message_creator() {
    register_active_or_paused_therapy_info(on_active_or_paused_therapy_existed);
    ESP_LOGI(TAG, "1111");

    register_on_write_records_feedback_callback(on_records_feedback);
    ESP_LOGI(TAG, "222");

    register_on_write_updating_records_callback(on_write_of_record_request_message);
    ESP_LOGI(TAG, "333");

    register_on_record_pending_approval_timeout_callback(on_record_pending_approval_timeout);
    ESP_LOGI(TAG, "4444");

    final_therapy_id_to_be_sent = read_therapy_count();
    ESP_LOGI(TAG, "final_therapy_id_to_be_sent: %u", final_therapy_id_to_be_sent);
}

