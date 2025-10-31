
#include <stdint.h>
#include "esp_err.h"

#ifndef DEFAULT_CONFIGURATION_HANDLER_H
#define DEFAULT_CONFIGURATION_HANDLER_H

void init_default_configuration_handler(void);
uint16_t get_default_therapy_duration(void);
const uint8_t* get_default_brightness(void);
esp_err_t set_and_store_default_therapy_duration(uint16_t seconds);
esp_err_t set_and_store_default_brightness(const uint8_t br[6]);
#endif