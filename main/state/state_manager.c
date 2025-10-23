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

#define MAX_STATE_LISTENERS 5

static const char* TAG = "StateManager";

static DeviceState current_state = STATE_IDLE;
static bool helmet_state = false;
static SemaphoreHandle_t state_mutex = NULL;
static state_change_callback state_listeners[MAX_STATE_LISTENERS];
QueueHandle_t state_event_queue;

static int state_listener_count = 0;

const char* get_device_state_str(DeviceState state) {
    switch (state) {
        case STATE_TEMPERATURE_ALERT: return "TemperatureAlert";
        case STATE_HUMIDITY_ALERT: return "HumidityAlert";
        case STATE_ACTIVE: return "Active";
        case STATE_INACTIVE: return "Inactive";
        case STATE_IDLE: return "Idle";
        default: return "Invalid";
    }
}

static void state_event_dispatcher_task(void *param) {
    DeviceState received_state;

    while (1) {
        if (xQueueReceive(state_event_queue, &received_state, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < state_listener_count; i++) {
                if (state_listeners[i]) {
                    state_listeners[i](received_state);
                }
            }
        }
    }
}

void init_state_manager() {
    state_mutex = xSemaphoreCreateMutex();
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for device state");
        return;
    }

    current_state = STATE_IDLE;

    state_event_queue = xQueueCreate(10, sizeof(DeviceState));
    if (state_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state event queue");
        return;
    }

    xTaskCreatePinnedToCore(state_event_dispatcher_task, "state_dispatcher", 3072, NULL, 5, NULL, tskNO_AFFINITY);
}

bool set_helmet_state(bool state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        helmet_state = state;
        ESP_LOGI(TAG, "Helmet state changed: %s", helmet_state ? "TRUE": "FALSE");

        xSemaphoreGive(state_mutex);
        return true;
    }
    return false;
}

void set_device_state(DeviceState new_state) {
    if (new_state > STATE_IDLE) {
        ESP_LOGW(TAG, "Trying to set invalid state");
        return;
    }
    if (state_mutex == NULL) {
        ESP_LOGE(TAG, "State mutex is NULL!");
    }
    else{
        ESP_LOGI(TAG, "State mutex is accessible!");
    }

    DeviceState to_send = (DeviceState)-1;

    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (new_state != current_state || new_state == STATE_ACTIVE) {
            DeviceState prev_state = current_state;
            current_state = new_state;
            to_send = new_state;
            ESP_LOGI(TAG, "Device state changed: %s -> %s", get_device_state_str(prev_state), get_device_state_str(new_state));
        }
        
        xSemaphoreGive(state_mutex);
    }

    else {
        ESP_LOGE(TAG, "Failed to acquire mutex to set device state");
        return;
    }

    if ((int)to_send != -1) {
        xQueueSend(state_event_queue, &to_send, portMAX_DELAY);
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
    //return true;
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