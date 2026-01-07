#include <stdint.h>

#ifndef MAIN_BUTTON_CONTROLLER_H
#define MAIN_BUTTON_CONTROLLER_H

typedef enum {
    SHORT = 0x01,
    LONG,
} ButtonPressType;

void wait_for_button_to_sleep(void *pvParameters);
void register_button_press_callback(void (*callback)(ButtonPressType));

#endif