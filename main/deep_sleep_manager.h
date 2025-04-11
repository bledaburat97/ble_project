#include <stdint.h>

#ifndef DEEP_SLEEP_MANAGER_H
#define DEEP_SLEEP_MANAGER_H

#define BUTTON_GPIO GPIO_NUM_0
#define BUTTON_PIN_BITMASK (1ULL << BUTTON_GPIO)
#define PRESS_DURATION_TO_SLEEP_MS 3000

void enter_deep_sleep();

#endif 