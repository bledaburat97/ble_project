#include "device_info_message_creator.h"

#include "message_queue_manager.h"

#include "../helper/binary_message_parser.h"
#include "../helper/binary_message_encoder.h"

#include "../storage/log_types.h"
#include "../storage/profile_partition_manager.h"
#include "../storage/therapy_counter.h"

#include "../ble/include/ble_connection_state_manager.h"
#include "../ble/include/ble_controller.h"

#include "../manager/timer_info_getter.h"
#include "../manager/current_therapy_state_manager.h"
#include "../manager/therapy_id_manager.h"
#include "../manager/session_timer_getter.h"
#include "../manager/message_saver.h"
#include "../manager/profile_manager.h"

#include "../device_configuration.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_log.h"

static const char *TAG = "DeviceInfoMessageCreator";
static void (*change_in_profile_id_during_active_callback)() = NULL;

// CCCD'ler hazır olunca BLE_CONNECTED log'u ekler flasha.
static void on_connect_ble() {
    ESP_LOGI(TAG, "On CCCD ready");
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = save_log(BLE_CONNECTED, NULL, 0, passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist BLE connected notification: %s", esp_err_to_name(err));
    }
    
    restart_duration_update_watchdog_timer();
}

static void send_device_info(uint16_t first_stored_therapy_id, uint16_t last_stored_therapy_id) {
    DeviceInfoMessage message = {0};
    //esp_err_t err = esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);

    message.first_stored_therapy_id = first_stored_therapy_id;
    message.last_stored_therapy_id = last_stored_therapy_id;
    uint8_t buf[DEVICE_INFO_SIZE];
    size_t len = encode_device_info_message_binary(&message, buf);
    send_info_message_to_queue(DEVICE_INFO_MESSAGE, buf, len);
}

static void on_write_of_profile_info_message(const uint8_t *buf, size_t len) {
    ESP_LOGI(TAG, "Received Profile Info Message");

    //profil id
    ProfileInfoMessage profile_info_message;
    if(!decode_profile_info_message_bin(buf, len, &profile_info_message)) {
        return;
    }

    uint32_t incoming_profile_id = profile_info_message.profile_id;
    uint16_t last_in_app = profile_info_message.last_therapy_id;

    uint16_t last_saved = read_therapy_count();

    ESP_LOGI(TAG, "last therapy id in app: %d", last_in_app);
    ESP_LOGI(TAG, "last therapy id saved: %d", last_saved);

    ProfileEntry last_entry;
    bool has_last = profile_read_last(&last_entry);
    if(!has_last) {
        ESP_LOGI(TAG, "There is no saved profile.");
    }
    uint16_t start = last_in_app + 1;
    uint16_t final_therapy_id_to_be_sent = last_saved;

    bool same_as_last = has_last && (last_entry.profile_id == incoming_profile_id);
    bool first_profile = !has_last;
    if(has_last && !same_as_last) {
        ESP_LOGI(TAG, "Incoming profile is %lu, last saved profile: %lu", incoming_profile_id, last_entry.profile_id);
    }
    set_profile_info(incoming_profile_id, !same_as_last, first_profile);

    if (!has_last || last_entry.profile_id == incoming_profile_id) {

        if (get_current_therapy_state() == ACTIVE || get_current_therapy_state() == PAUSED) {
            final_therapy_id_to_be_sent = (last_saved > 0) ? (uint16_t)(last_saved - 1u) : 0;
        } else {
            final_therapy_id_to_be_sent = last_saved;
        }

        if (final_therapy_id_to_be_sent == 0) {
            ESP_LOGI(TAG, "No finished therapy to send");
            send_device_info(0, 0);
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
            send_device_info(start, final_therapy_id_to_be_sent);
        }
        else{
            send_device_info(0, 0);
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
            send_device_info(0, 0);
            return;
        }
    }

    ProfileEntry found;
    int found_index = -1;
    if (!profile_find_last_occurrence(incoming_profile_id, &found, &found_index)) {
        send_device_info(0, 0);
        return;
    }

    ProfileEntry next;
    bool has_next = profile_read_at(found_index + 1, &next);
    if(!has_next) {
        ESP_LOGE(TAG, "There should have been a different profile which is connected after this one.");
        send_device_info(0, 0);
        return;
    }

    uint16_t w_start = found.therapy_id;
    final_therapy_id_to_be_sent = next.therapy_id - 1;

    if (w_start == 0) {
        ESP_LOGE(TAG, "There is no therapy with id of 0.");
        send_device_info(0, 0);
        return;
    }

    if (last_in_app < w_start) {
        start = w_start;
    }
    else if (last_in_app < final_therapy_id_to_be_sent) {
        start = last_in_app + 1;
    }
    else {
        send_device_info(0, 0);
        return;
    }

    if (final_therapy_id_to_be_sent - start >= MAX_RECORDS_TO_BE_SENT) {
        start = final_therapy_id_to_be_sent - MAX_RECORDS_TO_BE_SENT + 1;
    }

    send_device_info(start, final_therapy_id_to_be_sent);
}

void register_on_change_in_profile_id_during_active(void (*callback)()) {
    change_in_profile_id_during_active_callback = callback;
}

void init_device_info_message_creator() {
    register_on_write_profile_info_callback(on_write_of_profile_info_message);
    register_on_connect_callback(on_connect_ble);
}