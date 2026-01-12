#include "message_saver.h"

#include "esp_log.h"

#include "../storage/log_types.h"
#include "../storage/log_orchestrator.h"
#include "../storage/therapy_counter.h"

static bool s_can_continue_uncompleted = false;

#define TAG "MessageSaver"

void set_continue_uncompleted_therapy(bool status)
{
    s_can_continue_uncompleted = status;
}

esp_err_t save_log(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds) {
    uint16_t therapy_count = read_therapy_count();

    if ((type == TIMER_STATE_NEW_THERAPY_BY_BUTTON || type == TIMER_STATE_NEW_THERAPY_BY_APP)) {
        if (passed_seconds == 0) {
            if (therapy_count > 0) {
                esp_err_t finalize_error = finalize_old_slot(therapy_count);
                if (finalize_error != ESP_OK) {
                    ESP_LOGE(TAG, "Old slot could not be finalized.");
                    return ESP_FAIL;
                }
            }

            uint16_t new_therapy_count = therapy_count + 1;
            esp_err_t count_err = write_therapy_count(new_therapy_count);
            if (count_err != ESP_OK) {
                ESP_LOGE(TAG, "Therapy count could not be increased.");
            }

            return log_orchestrator_flush_logs(type, data, data_len, passed_seconds, therapy_count, true);
        }
        ESP_LOGE(TAG, "Passed seconds is not zero.");
        return ESP_FAIL;
    }

    else if (s_can_continue_uncompleted && (type == TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON || type == TIMER_STATE_CONTINUE_THERAPY_BY_APP)) {
        s_can_continue_uncompleted = false;

        if (therapy_count == 0) {
            ESP_LOGE(TAG, "CONTINUE_THERAPY log is received but therapy_count is 0");
            return ESP_FAIL;
        }
        return log_orchestrator_flush_logs(type, data, data_len, passed_seconds, therapy_count, false);
    }

    else {
        return log_orchestrator_add_log(type, data, data_len, passed_seconds, therapy_count);
    }
}