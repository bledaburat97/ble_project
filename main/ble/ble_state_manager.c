#include "ble_state_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

static SemaphoreHandle_t ble_mutex = NULL;
static bool ble_connection_status = false;
static const char *TAG = "BLEStateManager";

void set_ble_connection_status(bool status) {
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ble_connection_status = status;
        ESP_LOGI(TAG, "ble_connection_status updated to: %s", status ? "true" : "false");
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status update");
    }
}

bool get_ble_connection_status() {
    bool status = false;
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = ble_connection_status;
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status read");
    }
    return status;
}

SemaphoreHandle_t get_ble_mutex_handle() {
    return ble_mutex;
}

void init_ble_state_manager() {
    ble_mutex = xSemaphoreCreateMutex();
    if (!ble_mutex) {
        ESP_LOGE(TAG, "Failed to create BLE mutex");
    }
}
