#include "log_utils.h"
#include "log_types.h"
#include "esp_log.h"

#define TAG "LogUtils"
#define LOG_PARTITION_NAME "log_storage"

static const esp_partition_t* log_partition = NULL;

uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i]; // XOR
        for (uint8_t j = 0; j < 8; ++j) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07; // 0x07 = CRC-8 polynomial
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t init_log_writer() {
    log_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, LOG_STORAGE_SUBTYPE, LOG_PARTITION_NAME);

    if (!log_partition) {
        ESP_LOGE(TAG, "Log partition is not found");
        return ESP_FAIL;
    }
    return ESP_OK;
}

size_t get_log_entry_size(uint8_t type) {
    switch (type) {
        case REGIONS_BRIGHTNESS_UPDATED:   return sizeof(Regions_updated_t);
        case MEASUREMENT_CHANGED:  return sizeof(Measurement_changed_t);
        case THERAPY_STARTED_BY_BUTTON:
        case THERAPY_CONTINUED_BY_BUTTON:
        case THERAPY_STARTED_BY_APP:
        case THERAPY_CONTINUED_BY_APP:{
            return sizeof(Therapy_initialization_t);
        }
        case RTC_TIME_SAVED:  return sizeof(Time_saved_t);
        default: return sizeof(Notification_t);
    }
}

const esp_partition_t* get_log_partition() {
    return log_partition;
}
