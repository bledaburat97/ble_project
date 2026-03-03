#ifndef BRIGHTNESS_PARTITION_MANAGER_H
#define BRIGHTNESS_PARTITION_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    uint8_t default_brightness[6];
    uint8_t crc;
} DefaultBrightnessEntry;

esp_err_t init_brightness_partition(void);
bool default_brightness_read_last(DefaultBrightnessEntry *out);
bool default_brightness_append(const uint8_t brightness[6]);

#endif