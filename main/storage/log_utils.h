#pragma once

#include <stdint.h>
#include <stddef.h>
#include "storage/log_types.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t calculate_crc8(const uint8_t *data, size_t length);
BaseLogEntry fill_base_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds);
LogEntrySizeInfo get_log_entry_size_info(uint8_t type);

#ifdef __cplusplus
}
#endif
