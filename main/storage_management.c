#include "esp_log.h"
#include "storage_management.h"

static const char *NVS_NAMESPACE = "storage";
static const char *TAG = "StorageManager";

esp_err_t save_parameter(const char *key, void *value, size_t value_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing. Error: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }

    // Save the value to NVS based on its size
    if (value_size == sizeof(uint32_t)) {
        err = nvs_set_u32(nvs_handle, key, *(uint32_t *)value);
    } else if (value_size == sizeof(uint16_t)) {
        err = nvs_set_u16(nvs_handle, key, *(uint16_t *)value);
    } else if (value_size == sizeof(float)) {
        err = nvs_set_blob(nvs_handle, key, value, sizeof(float));
    } else {
        ESP_LOGE(TAG, "Unsupported value size for key: %s", key);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_ARG;
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit key '%s'.", key);
        }
    } else {
        ESP_LOGE(TAG, "Failed to save key '%s'. Error: %s", key, esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}


esp_err_t read_parameter(const char *key, void *value, size_t value_size) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for reading.");
        return err;
    }

    // Read the value from NVS based on its size
    if (value_size == sizeof(uint32_t)) {
        err = nvs_get_u32(nvs_handle, key, (uint32_t *)value);
    } else if (value_size == sizeof(uint16_t)) {
        err = nvs_get_u16(nvs_handle, key, (uint16_t *)value);
    } else if (value_size == sizeof(float)) {
        size_t required_size = sizeof(float);
        err = nvs_get_blob(nvs_handle, key, value, &required_size);  
        if (err == ESP_OK && required_size != sizeof(float)) {
            ESP_LOGE(TAG, "Incorrect float size for key: %s", key);
            err = ESP_ERR_INVALID_SIZE;
        }
    } else {
        ESP_LOGE(TAG, "Unsupported value size for key: %s", key);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_ARG;
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Read key '%s' successfully.", key);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Key '%s' not found in NVS.", key);
    } else {
        ESP_LOGE(TAG, "Failed to read key '%s'.", key);
    }

    nvs_close(nvs_handle);
    return err;
}