#ifndef MESSAGE_SAVER_H
#define MESSAGE_SAVER_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

void set_continue_uncompleted_therapy(bool status);
esp_err_t save_log(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds);

#endif