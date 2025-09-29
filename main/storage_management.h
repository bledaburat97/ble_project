#include <stdint.h>

#ifndef STORAGE_MANAGEMENT_H
#define STORAGE_MANAGEMENT_H

esp_err_t init_nvs();
esp_err_t save_parameter(const char *key, void *value, size_t value_size);
esp_err_t read_parameter(const char *key, void *value, size_t value_size);
esp_err_t save_parameter_u32(const char *key, uint32_t value);
esp_err_t read_parameter_u32(const char *key, uint32_t *out);
#endif