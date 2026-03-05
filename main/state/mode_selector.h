#ifndef MODE_SELECTOR_H
#define MODE_SELECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../device_configuration.h"

void initialize_mode_indicator_gpio();
void change_mode_indicator_gpio_pin_status();
bool get_indicator_led_status();
void change_default_parameters(uint16_t press_count);

#endif 