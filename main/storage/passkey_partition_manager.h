#ifndef PASSKEY_PARTITION_MANAGER_H
#define PASSKEY_PARTITION_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    uint32_t passkey;
    uint8_t crc;
} PasskeyEntry;

esp_err_t init_passkey_partition();
bool passkey_read_last(PasskeyEntry *out);
bool passkey_append(uint32_t passkey);
bool passkey_read_first(PasskeyEntry *out);

#endif