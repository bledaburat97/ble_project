#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

esp_err_t init_nvs();
esp_err_t save_parameter_u32(const char *key, uint32_t value);
esp_err_t read_parameter_u32(const char *key, uint32_t *out);
esp_err_t save_parameter_u16(const char *key, uint16_t value);
esp_err_t read_parameter_u16(const char *key, uint16_t *out);
esp_err_t save_parameter_blob(const char *key, const void *data, size_t len);
esp_err_t read_parameter_blob(const char *key, void *out, size_t *len_inout);

#endif