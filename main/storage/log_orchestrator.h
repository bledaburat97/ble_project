// log_orchestrator.h
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t log_orchestrator_init(void);
esp_err_t finalize_old_slot(uint16_t therapy_id);
esp_err_t log_orchestrator_flush_logs(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds, uint16_t therapy_id, bool is_first_log_to_append);
esp_err_t log_orchestrator_add_log(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds, uint16_t therapy_id);

#ifdef __cplusplus
}
#endif
