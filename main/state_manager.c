#include "state_manager.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "stdint.h"

#ifndef UNIT_TESTING
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#include "fake_freertos.h"
#include "fake_semphr.h"
#endif

static const char* TAG = "StatusController";

static DeviceState current_state = STATE_IDLE;
static bool helmet_state = false;
static SemaphoreHandle_t state_mutex = NULL;
static state_change_callback state_listeners[MAX_STATE_LISTENERS];
static helmet_state_change_callback helmet_state_listeners[MAX_STATE_LISTENERS];

static int state_listener_count = 0;
static int helmet_state_listener_count = 0;

const char* get_device_state_str(DeviceState state) {
    switch (state) {
        case STATE_TEMPERATURE_ALERT: return "TemperatureAlert";
        case STATE_HUMIDITY_ALERT: return "HumidityAlert";
        case STATE_ACTIVE: return "Active";
        case STATE_INACTIVITY: return "Inactivity";
        case STATE_START: return "Start";
        case STATE_IDLE: return "Idle";
        default: return "Invalid";
    }
}

void init_state_manager() {
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for device state");
    } else {
        current_state = STATE_IDLE;
    }
}

void set_helmet_state(bool state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        helmet_state = state;
        ESP_LOGI(TAG, "Helmet state changed: %s", helmet_state ? "TRUE": "FALSE");
        
        for (int i = 0; i < helmet_state_listener_count; i++) {
            if (helmet_state_listeners[i]) {
                helmet_state_listeners[i](helmet_state);
            }
        }

        xSemaphoreGive(state_mutex);
    }
}

void set_device_state(DeviceState new_state) {
    if (new_state > STATE_IDLE) {
        ESP_LOGW("DeviceState", "Trying to set invalid state");
        return;
    }

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (new_state != current_state || new_state == STATE_ACTIVE) {
            DeviceState prev_state = current_state;
            current_state = new_state;
            ESP_LOGI(TAG, "Device state changed: %s -> %s", get_device_state_str(prev_state), get_device_state_str(new_state));
    
            for (int i = 0; i < state_listener_count; i++) {
                if (state_listeners[i]) {
                    state_listeners[i](new_state);
                }
            }
        }
        else {
            ESP_LOGW(TAG, "Device state remains unchanged as: %s", get_device_state_str(new_state));
        }
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to set device state");
    }
}

DeviceState get_device_state() {
    DeviceState state_copy = STATE_IDLE;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        state_copy = current_state;
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to get device state");
    }
    return state_copy;
}

bool get_helmet_state() {
    bool helmet_state_copy = false;
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        helmet_state_copy = helmet_state;
        xSemaphoreGive(state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to get helmet state");
    }
    return helmet_state_copy;
}


void register_state_change_callback(state_change_callback callback) {
    if (state_listener_count < MAX_STATE_LISTENERS) {
        state_listeners[state_listener_count++] = callback;
        ESP_LOGI(TAG, "Registered state change listener (%d total)", state_listener_count);
    } else {
        ESP_LOGW(TAG, "Max state change listeners reached.");
    }
}

void register_helmet_state_change_callback(helmet_state_change_callback callback) {
    if (helmet_state_listener_count < MAX_STATE_LISTENERS) {
        helmet_state_listeners[helmet_state_listener_count++] = callback;
        ESP_LOGI(TAG, "Registered helmet state change listener (%d total)", helmet_state_listener_count);
    } else {
        ESP_LOGW(TAG, "Max state change listeners reached.");
    }
}