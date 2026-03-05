#ifndef DURATION_PARTITION_MANAGER_H
#define DURATION_PARTITION_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    uint16_t duration;
    uint8_t crc;
} DurationEntry;

esp_err_t init_default_duration_partition();
bool default_duration_read_last(DurationEntry *out);
bool default_duration_append(uint16_t duration);

#endif