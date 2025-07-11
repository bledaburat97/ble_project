#pragma once

#include <stdint.h>
#include <stddef.h>
#include "log_types.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THERAPY_SLOT_SIZE 4096
#define MAX_SAVED_THERAPY 500

#define LOG_STORAGE_SUBTYPE 0x82

uint8_t calculate_crc8(const uint8_t *data, size_t length);
BaseLogEntry fill_base_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds);
esp_err_t init_log_writer();
const esp_partition_t* get_log_partition();
LogEntrySizeInfo get_log_entry_size_info(uint8_t type);

#ifdef __cplusplus
}
#endif
