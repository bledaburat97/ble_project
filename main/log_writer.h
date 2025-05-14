#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


esp_err_t log_writer_init();
esp_err_t add_log(const void* entry, size_t size);
void print_cached_log_sizes();
void test_cache_log_limit();
void test_flush_to_slot();
void test_slot_fill();
void test_add_log_flow();
void erase_therapy_partition(uint32_t offset);
#ifdef __cplusplus
}
#endif
