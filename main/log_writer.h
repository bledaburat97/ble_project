#pragma once

#include "esp_err.h"
#include "log_types.h"

#ifdef __cplusplus
extern "C" {
#endif


esp_err_t log_writer_init();
esp_err_t add_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds);
esp_err_t add_notification_log(uint8_t type, uint16_t passed_seconds);
void print_cached_log_sizes();
void test_cache_log_limit();
void test_flush_to_slot();
void test_slot_fill();
void test_add_log_flow();
void erase_therapy_partition(uint32_t offset);
uint16_t get_last_saved_passed_duration();
esp_err_t read_logs_and_encode(uint16_t therapy_id);

#ifdef __cplusplus
}
#endif
