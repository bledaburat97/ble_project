#include "current_therapy_info_manager.h"

#include <stdint.h>
#include "storage/log_writer.h"
#include "therapy_counter.h"
#include "esp_log.h"
#include "matching_message_encoder.h"
#include "timer_management.h"
#include "state_manager.h"

static const char *TAG = "CurrentTherapyInfoManager";

typedef enum {
    ACTIVE = 0,
    PAUSED,
    NONE,
} CurrentTherapyState;

static CurrentTherapyState current_therapy_state = NONE;
static uint16_t active_therapy_duration = 0;
static uint16_t passed_duration = 0;
static uint8_t active_brightness[6];

static void on_timer_start(DeviceState device_state) {
    switch(device_state) {
        case STATE_TEMPERATURE_ALERT:
        case STATE_INACTIVE:
            if(current_therapy_state == ACTIVE) {
                current_therapy_state = PAUSED;
            }
            break;
        case STATE_ACTIVE:
            current_therapy_state = ACTIVE;
            break;
        default:
            break;
    }
}
/*
        case NOTIF_SHUT_DOWN_BY_BUTTON:
        case NOTIF_THERAPY_STOPPED_BY_APP:
        case NOTIF_THERAPY_COMPLETED:
            break;
*/
void init_current_therapy_info_manager() {
    register_timer_start_callback(on_timer_start);

    uint16_t therapy_id = read_therapy_count();
    if(therapy_id == 0) {
        ESP_LOGE(TAG, "There should be a saved therapy.");
        return;
    }
    ReadTherapyInfo therapy_info;
    if(read_therapy_info(therapy_id, &therapy_info)) {
        if(therapy_info.is_over) {
            current_therapy_state = NONE;
            return;
        }
        active_therapy_duration = therapy_info.therapy_duration;
        memcpy(active_brightness, therapy_info.brightness, sizeof(active_brightness));
        passed_duration = therapy_info.passed_duration;
        current_therapy_state = PAUSED;
    }
}

void set_records(uint16_t therapy_id) {
    ESP_LOGI(TAG, "Set records");
    if(current_therapy_state == NONE && therapy_id == read_therapy_count()) {
        ESP_LOGE(TAG, "HATA.");
        return;
    }
    if(therapy_id == 0) {
        ESP_LOGE(TAG, "There should be a saved therapy.");
        return;
    }
    ReadTherapyLogs read_therapy_logs;
    if(read_records(therapy_id, &read_therapy_logs)) {
        init_fragments();

        start_encoding_for_new_therapy(therapy_id, active_therapy_duration, passed_duration);

        if (read_therapy_logs.count_measurements > 0) {
            ESP_LOGI(TAG, "aaaa, count_measurements: %u", read_therapy_logs.count_measurements);
            encode_records_of_therapy(therapy_id, 0x03, 4, read_therapy_logs.count_measurements, read_therapy_logs.measurements);
        }
        if (read_therapy_logs.count_notifications > 0) {
            ESP_LOGI(TAG, "bbbb, count_notifications: %u", read_therapy_logs.count_notifications);
            encode_records_of_therapy(therapy_id, 0x04, 3, read_therapy_logs.count_notifications, read_therapy_logs.notifications);
        }
        if (read_therapy_logs.count_brightness > 0) {
            ESP_LOGI(TAG, "cccc, count_brightness: %u", read_therapy_logs.count_brightness);
            encode_records_of_therapy(therapy_id, 0x05, 8, read_therapy_logs.count_brightness, read_therapy_logs.brightness_updates);
        }

        free(read_therapy_logs.measurements);
        free(read_therapy_logs.notifications);
        free(read_therapy_logs.brightness_updates);
    }
}