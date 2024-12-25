#include <stdint.h>
#include "nvs_flash.h"

#ifndef STORAGE_MANAGEMENT_H
#define STORAGE_MANAGEMENT_H

esp_err_t save_parameter(const char *key, void *value, size_t value_size);
esp_err_t read_parameter(const char *key, void *value, size_t value_size);


#endif