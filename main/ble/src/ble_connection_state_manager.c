#include "../include/ble_connection_state_manager.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stdbool.h>

static SemaphoreHandle_t ble_mutex = NULL;
static bool ble_connection_status = false;
static const char *TAG = "BLEConnectionStateManager";

// BLE bağlantı durumunu thread-safe şekilde günceller.
void set_ble_connection_status(bool status) {
    if (ble_mutex == NULL) {
        ESP_LOGE(TAG, "ble mutex is null, cannot update connection status");
        return;
    }
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ble_connection_status = status;
        ESP_LOGI(TAG, "ble_connection_status updated to: %s", status ? "true" : "false");
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status update");
    }
}

// BLE bağlantı durumunu thread-safe şekilde okur.
bool get_ble_connection_status() {
    if (ble_mutex == NULL) {
        ESP_LOGE(TAG, "ble mutex is null, cannot read connection status");
        return false;
    }
    bool status = false;
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = ble_connection_status;
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status read");
    }
    return status;
}

// BLE gönderimleri için ortak mutex handle'ını döndürür.
SemaphoreHandle_t get_ble_mutex_handle() {
    return ble_mutex;
}

// BLE bağlantı durumu için mutex'i bir kez oluşturur.
void init_ble_state_manager() {
    if (ble_mutex == NULL) {
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
