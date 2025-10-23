#include "log_partition_manager.h"

#include "esp_log.h"
#include <string.h>

#define LOG_PARTITION_NAME "log_storage"
#define LOG_STORAGE_SUBTYPE 0x82
#define TAG "LogPartitionManager"

static const esp_partition_t* log_partition = NULL;

esp_err_t init_log_partition() {
    log_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, LOG_STORAGE_SUBTYPE, LOG_PARTITION_NAME);

    if (!log_partition) {
        ESP_LOGE(TAG, "Log partition is not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Log partition is successfuly initiated.");

    return ESP_OK;
}

const esp_partition_t* get_log_partition() {
    return log_partition;
}