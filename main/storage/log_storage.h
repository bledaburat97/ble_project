#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * log_storage:
 * - Flash partition erişimi tek noktadan
 * - Read/Write/Erase
 * - Opsiyonel global mutex (reader + writer çakışmasını kesmek için)
 */

esp_err_t log_storage_init(void);

/** Global lock (multi-step işlemlerde orchestrator kullanabilir) */
void log_storage_lock(void);
void log_storage_unlock(void);

esp_err_t log_storage_read(uint32_t offset, void *out, size_t len);
esp_err_t log_storage_write(uint32_t offset, const void *data, size_t len);
esp_err_t log_storage_erase(uint32_t offset, size_t len);

/** Slot helper’ları */
esp_err_t log_storage_read_slot(uint32_t base_offset, uint8_t *out_buf, size_t buf_len);
esp_err_t log_storage_erase_slot(uint32_t base_offset);

bool log_storage_is_ready(void);

#ifdef __cplusplus
}
#endif