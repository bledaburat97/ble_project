#include "timer_utils.h"

#include "esp_log.h"
#include "stdint.h"

static const char* TAG = "TimerUtils";

TimerHandle_t create_and_start_timer(DeviceState state, uint32_t duration_ms, TimerCallbackFunction_t callback) {
    const char* name = get_device_state_str(state);

    const TickType_t ticks = pdMS_TO_TICKS(duration_ms);

    TimerHandle_t timer = xTimerCreate(name, ticks, pdFALSE, NULL, callback);
    if (timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer: %s", name);
        return NULL;
    }

    xTimerStop(timer, 0);
    xTimerChangePeriod(timer, ticks, 0);

    if (xTimerStart(timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer: %s", name);
        xTimerDelete(timer, 0);
        return NULL;
    }

    ESP_LOGI(TAG, "Timer %s started (%ld ms)", name, duration_ms);
    return timer;
}

bool stop_and_delete_timer(TimerHandle_t* timer) {

    if (!timer || !*timer) return false;

    if (xTimerStop(*timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Timer stopped");
    }
    else {
        ESP_LOGE(TAG, "Failed to stop timer.");
    }
   
    if (xTimerDelete(*timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Timer deleted");
        *timer = NULL;
    }
    else{
        ESP_LOGE(TAG, "Failed to delete timer.");
        return false;
    }
    return true;
}