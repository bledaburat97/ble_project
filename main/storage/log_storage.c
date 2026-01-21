// log_storage.c
#include "log_storage.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "log_partition_manager.h"   // sadece burada!
#include "log_config.h"

#define TAG "LogStorage"

static const esp_partition_t *s_part = NULL;
static SemaphoreHandle_t s_mutex = NULL;

esp_err_t log_storage_init(void)
{
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            ESP_LOGE(TAG, "Mutex create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    s_part = get_log_partition();
    if (!s_part) {
        ESP_LOGE(TAG, "Log partition is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

bool log_storage_is_ready(void)
{
    return (s_part != NULL);
}

void log_storage_lock(void)
{
    if (!s_mutex) {
        // init edilmemiş olabilir; init dene
        (void)log_storage_init();
    }
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
}

void log_storage_unlock(void)
{
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

static inline esp_err_t ensure_partition(void)
{
    if (!s_part) {
        s_part = get_log_partition();
        if (!s_part) return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static inline esp_err_t ensure_bounds(uint32_t offset, size_t len)
{
    if (!s_part) return ESP_ERR_INVALID_STATE;
    if (len == 0) return ESP_ERR_INVALID_ARG;
    if (offset >= s_part->size) return ESP_ERR_INVALID_SIZE;
    if (len > (size_t)(s_part->size - offset)) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

esp_err_t log_storage_read(uint32_t offset, void *out, size_t len)
{
    if (!out || len == 0) return ESP_ERR_INVALID_ARG;

    esp_err_t e = ensure_partition();
    if (e != ESP_OK) return e;
    e = ensure_bounds(offset, len);
    if (e != ESP_OK) return e;

    return esp_partition_read(s_part, offset, out, len);
}

esp_err_t log_storage_write(uint32_t offset, const void *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    esp_err_t e = ensure_partition();
    if (e != ESP_OK) return e;
    e = ensure_bounds(offset, len);
    if (e != ESP_OK) return e;

    return esp_partition_write(s_part, offset, data, len);
}

esp_err_t log_storage_erase(uint32_t offset, size_t len)
{
    if (len == 0) return ESP_ERR_INVALID_ARG;

    esp_err_t e = ensure_partition();
    if (e != ESP_OK) return e;
    e = ensure_bounds(offset, len);
    if (e != ESP_OK) return e;

    return esp_partition_erase_range(s_part, offset, len);
}

esp_err_t log_storage_read_slot(uint32_t base_offset, uint8_t *out_buf, size_t buf_len)
{
    if (!out_buf || buf_len < THERAPY_SLOT_SIZE) return ESP_ERR_INVALID_ARG;
    return log_storage_read(base_offset, out_buf, THERAPY_SLOT_SIZE);
}

esp_err_t log_storage_erase_slot(uint32_t base_offset)
{
    return log_storage_erase(base_offset, THERAPY_SLOT_SIZE);
}
