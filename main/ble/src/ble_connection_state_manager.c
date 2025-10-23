#include "../include/ble_connection_state_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

static SemaphoreHandle_t ble_mutex = NULL;
static bool ble_connection_status = false;
static const char *TAG = "BLEConnectionStateManager";

void set_ble_connection_status(bool status) {
    if (ble_mutex == NULL) {
        ESP_LOGI(TAG, "ble mutex is null.");
    }
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
    if (ble_mutex == NULL) { // Sadece NULL ise başlat
        ble_mutex = xSemaphoreCreateMutex();
        if (!ble_mutex) {
            ESP_LOGE(TAG, "Failed to create BLE mutex");
        } else {
            ESP_LOGI(TAG, "BLE mutex created successfully.");
        }
    } else {
        ESP_LOGW(TAG, "BLE mutex already initialized. Skipping creation.");
    }
}