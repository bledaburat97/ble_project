#include "state_manager.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "stdint.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_STATE_LISTENERS 5

static const char* TAG = "StateManager";

static DeviceState s_current_state = STATE_IDLE;
static bool s_helmet_state = false;
static SemaphoreHandle_t s_state_mutex = NULL;
static state_change_callback s_state_listeners[MAX_STATE_LISTENERS];
static QueueHandle_t s_state_event_queue;

static int s_state_listener_count = 0;

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
        if (xQueueReceive(s_state_event_queue, &received_state, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < s_state_listener_count; i++) {
                if (s_state_listeners[i]) {
                    s_state_listeners[i](received_state);
                }
            }
        }
    }
}

void init_state_manager() {
    s_state_mutex = xSemaphoreCreateMutex();
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for device state");
        return;
    }

    s_current_state = STATE_IDLE;

    s_state_event_queue = xQueueCreate(10, sizeof(DeviceState));
    if (s_state_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state event queue");
        return;
    }

    xTaskCreatePinnedToCore(state_event_dispatcher_task, "state_dispatcher", 3072, NULL, 5, NULL, tskNO_AFFINITY);
}

bool set_helmet_state(bool state) {
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        s_helmet_state = state;
        ESP_LOGI(TAG, "Helmet state changed: %s", s_helmet_state ? "TRUE": "FALSE");

        xSemaphoreGive(s_state_mutex);
        return true;
    }
    return false;
}

void set_device_state(DeviceState new_state) {
    if (new_state > STATE_IDLE) {
        ESP_LOGW(TAG, "Trying to set invalid state");
        return;
    }
    if (s_state_mutex == NULL) {
        ESP_LOGE(TAG, "State mutex is NULL!");
    }

    DeviceState to_send = (DeviceState)-1;

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (new_state != s_current_state || new_state == STATE_ACTIVE) {
            DeviceState prev_state = s_current_state;
            s_current_state = new_state;
            to_send = new_state;
            ESP_LOGI(TAG, "Device state changed: %s -> %s", get_device_state_str(prev_state), get_device_state_str(new_state));
        }
        
        xSemaphoreGive(s_state_mutex);
    }

    else {
        ESP_LOGE(TAG, "Failed to acquire mutex to set device state");
        return;
    }

    if ((int)to_send != -1) {
        xQueueSend(s_state_event_queue, &to_send, portMAX_DELAY);
    }
}

DeviceState get_device_state() {
    DeviceState state_copy = STATE_IDLE;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        state_copy = s_current_state;
        xSemaphoreGive(s_state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to get device state");
    }
    return state_copy;
}

bool get_helmet_state() {
    return true;
    bool helmet_state_copy = false;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        helmet_state_copy = s_helmet_state;
        xSemaphoreGive(s_state_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex to get helmet state");
    }
    return helmet_state_copy;
}

void register_state_change_callback(state_change_callback callback) {
    if (s_state_listener_count < MAX_STATE_LISTENERS) {
        s_state_listeners[s_state_listener_count++] = callback;
    } else {
        ESP_LOGW(TAG, "Max state change listeners reached.");
    }
}