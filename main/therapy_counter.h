#pragma once

#ifndef UNIT_TESTING
#include "esp_err.h"
#else
#include "fake_esp_err.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t init_therapy_counter_partition();
uint16_t read_therapy_count();
esp_err_t write_therapy_count(uint16_t new_count);
void erase_therapy_counter_partition();
#ifdef __cplusplus
}
#endif