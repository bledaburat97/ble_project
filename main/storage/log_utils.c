#include "log_utils.h"

#include "log_types.h"

#include "esp_log.h"
#include <string.h>

#define TAG "LogUtils"

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
    if(data_len != get_log_entry_size_info(type).data_length) {
        ESP_LOGE(TAG, "Wrong data length!, data_len: %u, entry_size: %u", data_len, get_log_entry_size_info(type).total_length);
        log.entry_size = 0;
        return log;
    }
    
    switch(type)
    {
        case PASSED_DURATION_UPDATED:
            log.can_be_cached = false;
            log.can_be_flashed = true;
            log.can_start_cache = false;
            break;
        case DEVICE_AWAKED:
            log.can_be_cached = true;
            log.can_be_flashed = false;
            log.can_start_cache = true;
            break;
        case NOTIF_THERAPY_COMPLETED:
        case NOTIF_THERAPY_STOPPED_BY_APP:
            log.can_be_cached = false;
            log.can_be_flashed = true;
            log.can_start_cache = true;
            break;
        case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
        case TIMER_STATE_NEW_THERAPY_BY_APP:
            log.can_be_cached = true;
            log.can_be_flashed = true;
            log.can_start_cache = false;
            break;
        default:
            log.can_be_cached = true;
            log.can_be_flashed = true;
            log.can_start_cache = false;
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
    memcpy(&crc_input[1], log.data, data_len);
    crc_input[1 + data_len] = (passed_seconds >> 8) & 0xFF;
    crc_input[1 + data_len + 1] = passed_seconds & 0xFF;
    log.crc = calculate_crc8(crc_input, log.entry_size - 1);
    return log;
}

LogEntrySizeInfo get_log_entry_size_info(uint8_t type) {
    //1 byte type
    //? byte data
    //2 byte passed_sec
    //1 byte crc
    LogEntrySizeInfo size_info;
    switch (type) {
        case NOTIF_BRIGHTNESS_UPDATED:
            size_info.total_length = 10;
            size_info.data_length = 6; //uint8_t region_brightnesses[6];
            break;
        case MEASUREMENT_CHANGED:
            size_info.total_length = 6;
            size_info.data_length = 2; //uint8_t temperature;uint8_t humidity;
            break;
        case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
        case TIMER_STATE_NEW_THERAPY_BY_APP:
        case TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON:
        case TIMER_STATE_CONTINUE_THERAPY_BY_APP:
            size_info.total_length = 10;
            size_info.data_length = 6; //uint16_t therapy_id;uint16_t therapy_duration; uint16_t therapy_passed_duration
            break;
        case RTC_TIME_SAVED:
            size_info.total_length = 9;
            size_info.data_length = 5; //uint8_t timestamp[5];
            break;
        default: 
            size_info.total_length = 4;
            size_info.data_length = 0;
            break;
    }
    return size_info;
}
