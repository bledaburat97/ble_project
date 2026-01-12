#include "therapy_id_manager.h"

#include "current_therapy_state_manager.h"

#include "../storage/therapy_counter.h"

#include "../transaction/default_configuration_handler.h"

#include "../i2c/laser/laser_driver_controller.h"

#include "../device_configuration.h"

#include <stdint.h>
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "TherapyIdManager";

uint16_t get_current_therapy_id(void)
{
    if (get_current_therapy_state() == NONE) {
        ESP_LOGI(TAG, "No active or paused therapy is present when requesting current therapy ID.");
        return 0;
    }

    uint16_t therapy_count = read_therapy_count();
    if (therapy_count == 0) {
        ESP_LOGW(TAG, "Therapy counter returned zero while a therapy state is active.");
    }

    return therapy_count;
}

uint16_t get_new_therapy_id_for_new_therapy(void)
{
    uint16_t therapy_count = read_therapy_count();
    if (therapy_count >= MAX_THERAPY_COUNT) {
        ESP_LOGW(TAG, "Therapy counter reached the maximum value: %u", therapy_count);
        return therapy_count;
    }
    return (uint16_t)(therapy_count + 1u);
}

uint16_t get_last_completed_therapy_id(void) {
    uint16_t therapy_count = read_therapy_count();
    if (get_current_therapy_state() == NONE) {
        return therapy_count;
    }
    if (therapy_count == 0) {
        ESP_LOGW(TAG, "Therapy counter returned zero while a session is active.");
        return 0;
    }
    return (uint16_t)(therapy_count - 1u);
}

