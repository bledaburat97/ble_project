#include <stdint.h>

#ifndef DEEP_SLEEP_MANAGER_H
#define DEEP_SLEEP_MANAGER_H

#define BUTTON_GPIO GPIO_NUM_0
#define BUTTON_PIN_BITMASK (1ULL << BUTTON_GPIO)

void enter_deep_sleep();
void wait_for_button_and_sleep_task(void *pvParameters);

#endif 