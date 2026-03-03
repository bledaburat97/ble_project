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
static uint16_t final_therapy_id_to_be_sent;
static SemaphoreHandle_t s_records_mutex;

static uint8_t s_records_slot_buf[THERAPY_SLOT_SIZE];

static void (*change_in_profile_id_during_active_callback)() = NULL;


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
        bool is_last = (i == fragment_count - 1);
        send_records_info_message_to_queue(therapy_id, (uint8_t*)frag, len, is_last);
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

// Aktif/paused terapi varsa records akışını tetikler.
static void on_active_or_paused_therapy_existed()
{
    if(is_record_pending()) {
        ESP_LOGE(TAG, "is_record_pending true");
        return;
    }
    uint16_t current_id = get_current_therapy_id();
    if (current_id == 0) {
        ESP_LOGW(TAG, "No active therapy id available while attempting to send records");
        return;
    }
    send_records_info_message(current_id, true);
}

// Uygulamadan gelen "record isteği"ni işler.
static void on_write_of_record_request_message(const uint8_t *buf, size_t len) {
    if(is_record_pending()) {
        ESP_LOGI(TAG, "is_record_pending true");
        return;
    }
    ESP_LOGI(TAG, "on_write_of_record_request_message");

    UpdateRecordRequestMessage record_request_message;
    if(!decode_update_record_request_message_bin(buf, len, &record_request_message)) {
        return;
    }

    uint32_t incoming_profile = record_request_message.profile_id;
    uint16_t last_in_app = record_request_message.last_therapy_id;

    uint16_t last_saved = read_therapy_count();

    ESP_LOGI(TAG, "last therapy id in app: %d", last_in_app);
    ESP_LOGI(TAG, "last therapy id saved: %d", last_saved);

    ProfileEntry last_entry;
    bool has_last = profile_read_last(&last_entry);
    if(!has_last) {
        ESP_LOGI(TAG, "There is no saved profile.");
    }
    uint16_t start = last_in_app + 1;

    bool same_as_last = has_last && (last_entry.profile_id == incoming_profile);
    bool first_profile = !has_last;
    if(has_last && !same_as_last) {
        ESP_LOGI(TAG, "Incoming profile is %lu, last saved profile: %lu", incoming_profile, last_entry.profile_id);
    }
    set_profile_info(incoming_profile, !same_as_last, first_profile);

    if (!has_last || last_entry.profile_id == incoming_profile) {

        if (get_current_therapy_state() == ACTIVE || get_current_therapy_state() == PAUSED) {
            final_therapy_id_to_be_sent = (last_saved > 0) ? (uint16_t)(last_saved - 1u) : 0;
        } else {
            final_therapy_id_to_be_sent = last_saved;
        }

        if (final_therapy_id_to_be_sent == 0) {
            ESP_LOGI(TAG, "No finished therapy to send");
            return;
        }

        uint16_t min_window_start = 1;
        if (final_therapy_id_to_be_sent >= MAX_RECORDS_TO_BE_SENT) {
            min_window_start = final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT + 1;
        }

        if (start < min_window_start) {
            start = min_window_start;
        }
        if (start <= final_therapy_id_to_be_sent) {
            send_records_info_message(start, false);
        }
        return;
    }

    // Case B: profile mismatch
    if (get_current_therapy_state() == ACTIVE || get_current_therapy_state() == PAUSED) {
        if (change_in_profile_id_during_active_callback) {
            change_in_profile_id_during_active_callback();
        }
        else{
            ESP_LOGE(TAG, "change_in_profile_id_during_active_callback is not found.");
            return;
        }
    }

    ProfileEntry found;
    int found_index = -1;
    if (!profile_find_last_occurrence(incoming_profile, &found, &found_index)) {
        // hiç yok → kayıt gönderme, sadece bağla
        return;
    }

    ProfileEntry next;
    bool has_next = profile_read_at(found_index + 1, &next);
    if(!has_next) {
        ESP_LOGE(TAG, "There should have been a different profile which is connected after this one.");
        return;
    }

    uint16_t w_start = found.therapy_id;
    final_therapy_id_to_be_sent = next.therapy_id - 1;

    if (w_start == 0) {
        ESP_LOGE(TAG, "There is no therapy with id of 0.");
        return;
    }

    if (last_in_app < w_start) {
        start = w_start;
    }
    else if (last_in_app < final_therapy_id_to_be_sent) {
        start = last_in_app + 1;
    }
    else { 
        return;
    }

    if (final_therapy_id_to_be_sent - start >= MAX_RECORDS_TO_BE_SENT) {
        start = final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT + 1;
    }

    send_records_info_message(start, false);
}

// Uygulamanın ACK/NACK geri bildirimi.
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
            send_records_info_message(therapy_id, false);
        }
    }
    else {
        send_records_info_message(records_feedback_message.therapy_id, false);
    }
}

// Timeout veya NACK durumunda aynı terapiyi tekrar gönderir.
static void send_record_again(const uint16_t therapy_id) {
    if (therapy_id == 0) {
        ESP_LOGW(TAG, "Cannot resend records for therapy id 0");
        return;
    }
    bool is_active_therapy = therapy_id == get_current_therapy_id();
    send_records_info_message(therapy_id, is_active_therapy);
}

// ACK sonrası bir sonraki terapiyi gönderir.
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
        send_records_info_message(new_therapy_id, false);
    }
}

// Records aktarım akışını başlatır ve callback'leri bağlar.
void init_records_info_message_creator() {
    s_records_mutex = xSemaphoreCreateMutex();
    register_active_or_paused_therapy_info(on_active_or_paused_therapy_existed);
    register_on_write_records_feedback_callback(on_records_feedback);
    register_on_write_updating_records_callback(on_write_of_record_request_message);
    register_send_record_again_callback(send_record_again);
    register_send_new_record_callback(send_new_record);

    uint16_t therapy_count = read_therapy_count();

    if(get_current_therapy_state() == NONE) {
        final_therapy_id_to_be_sent = therapy_count;
    }
    else {
        final_therapy_id_to_be_sent = (therapy_count > 0) ? (uint16_t)(therapy_count - 1u) : 0;
    }
    ESP_LOGI(TAG, "The therapy Id with %u will be the last record to be sent.", final_therapy_id_to_be_sent);
}

void register_on_change_in_profile_id_during_active(void (*callback)()) {
    change_in_profile_id_during_active_callback = callback;
}