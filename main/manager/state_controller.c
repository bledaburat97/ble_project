#include "state_controller.h"
#include "state_getter.h"

#include "../common/device_states.h"

#include "esp_log.h"
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "StateController";

// Cihazın mevcut durumu.
static DeviceState s_current_state = STATE_IDLE;
// Durum güncellemelerini koruyan mutex.
static SemaphoreHandle_t s_state_mutex = NULL;

// Cihaz durumunu thread-safe şekilde günceller.
void set_device_state(DeviceState new_state) {
    if (new_state > STATE_IDLE) {
        ESP_LOGW(TAG, "Trying to set invalid state=%d", (int)new_state);
        return;
    }

    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "State mutex is NULL!");
        return;
    }

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex to set device state");
        return;
    }

    if (new_state != s_current_state || new_state == STATE_ACTIVE) {
        DeviceState prev_state = s_current_state;
        s_current_state = new_state;

        ESP_LOGI(TAG, "Device state changed: %s -> %s",
                 get_device_state_str(prev_state),
                 get_device_state_str(new_state));
    }

    xSemaphoreGive(s_state_mutex);
}

// Cihaz durumunu thread-safe şekilde okur.
DeviceState get_device_state(void) {
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "State mutex is NULL!");
        return STATE_IDLE;
    }

    DeviceState state_copy = STATE_IDLE;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        state_copy = s_current_state;
        xSemaphoreGive(s_state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to get device state");
    }

    return state_copy;
}

void init_state_controller(void) {
    if (s_state_mutex != NULL) {
        ESP_LOGW(TAG, "init_state_controller called twice");
        return;
    }

    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for device state");
        return;
    }

    s_current_state = STATE_IDLE;
}
