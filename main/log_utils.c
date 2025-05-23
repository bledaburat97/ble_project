#include "log_utils.h"
#include "log_types.h"
#include "esp_log.h"
#include <string.h>

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


BaseLogEntry fill_base_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds) {
    BaseLogEntry log;
    if(data_len != get_log_entry_size(type) - 4) {
        ESP_LOGE(TAG, "Wrong data length!, data_len: %u, entry_size: %u", data_len, get_log_entry_size(type));
        log.entry_size = 0;
        return log;
    }
    
    switch(type)
    {
        case PASSED_DURATION_UPDATED:
            log.can_be_cached = false;
            log.can_be_flashed = true;
            log.can_start_cache = false;
            log.can_flush_cache = false;
            break;
        case DEVICE_AWAKED:
            log.can_be_cached = true;
            log.can_be_flashed = false;
            log.can_start_cache = true;
            log.can_flush_cache = false;
            break;
        case THERAPY_COMPLETED:
            log.can_be_cached = false;
            log.can_be_flashed = true;
            log.can_start_cache = true;
            log.can_flush_cache = false;
            break;
        case THERAPY_STARTED_BY_BUTTON:
        case THERAPY_STARTED_BY_APP:
            log.can_be_cached = true;
            log.can_be_flashed = true;
            log.can_start_cache = false;
            log.can_flush_cache = true;
            break;
        default:
            log.can_be_cached = true;
            log.can_be_flashed = true;
            log.can_start_cache = false;
            log.can_flush_cache = false;
            break;
    }
    
    
    log.type = type;

    if (data_len > 0) {
        if (data) {
            memcpy(log.data, data, data_len);
        } else {
            memset(log.data, 0, data_len);
        }
    }

    log.passed_seconds = passed_seconds;
    log.entry_size = data_len + 4;

    uint8_t crc_input[log.entry_size - 1];
    crc_input[0] = type;
    memcpy(&crc_input[1], data, data_len);
    crc_input[1 + data_len] = passed_seconds;
    log.crc = calculate_crc8(crc_input, log.entry_size - 1);
    return log;
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
        case TEST: return 5004;
        default: return sizeof(Notification_t);
    }
}

const esp_partition_t* get_log_partition() {
    return log_partition;
}
