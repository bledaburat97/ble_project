#ifndef PROFILE_PARTITION_MANAGER_H
#define PROFILE_PARTITION_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    uint32_t profile_id;
    uint16_t therapy_id;
    uint8_t crc;
} ProfileEntry;

esp_err_t init_profile_partition();
bool profile_read_last(ProfileEntry* out);
bool profile_find_last_occurrence(uint32_t profile_id, ProfileEntry* out, int* out_index);
bool profile_read_at(int index, ProfileEntry* out);
bool profile_append(uint32_t profile_id, uint16_t therapy_id);
esp_err_t erase_profile_partition(void);
#endif