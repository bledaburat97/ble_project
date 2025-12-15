#include "storage_manager.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "StorageManager";
static const char *NVS_NAMESPACE = "storage";

static void check_nvs_stats(void)
{
    nvs_stats_t stats;
    esp_err_t err = nvs_get_stats(NULL, &stats);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS statistics: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG,
             "NVS stats - Used: %u, Free: %u, Total: %u",
             stats.used_entries,
             stats.free_entries,
             stats.total_entries);
}

esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG,
                 "NVS partition needs erase (ret=0x%x). Erasing and reinitializing...",
                 ret);
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }

    check_nvs_stats();
    return ret;
}

esp_err_t save_parameter_u32(const char *key, uint32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (u32): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save u32 parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t read_parameter_u32(const char *key, uint32_t *out)
{
    if (out == NULL) {
        ESP_LOGE(TAG, "read_parameter_u32: output pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (u32): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u32(handle, key, out);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read u32 parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t save_parameter_u16(const char *key, uint16_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (u16): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save u16 parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t read_parameter_u16(const char *key, uint16_t *out)
{
    if (out == NULL) {
        ESP_LOGE(TAG, "read_parameter_u16: output pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (u16): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u16(handle, key, out);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read u16 parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t save_parameter_blob(const char *key, const void *data, size_t len)
{
    if (data == NULL && len > 0) {
        ESP_LOGE(TAG, "save_parameter_blob: data pointer is NULL while len=%u", (unsigned)len);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (blob): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, key, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save blob parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}

esp_err_t read_parameter_blob(const char *key, void *out, size_t *len_inout)
{
    if (len_inout == NULL) {
        ESP_LOGE(TAG, "read_parameter_blob: len pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' (blob): %s",
                 NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_blob(handle, key, out, len_inout);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read blob parameter '%s': %s", key, esp_err_to_name(err));
    }

    nvs_close(handle);
    return err;
}
