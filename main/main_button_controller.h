#include <stdint.h>

#ifndef MAIN_BUTTON_CONTROLLER_H
#define MAIN_BUTTON_CONTROLLER_H

void wait_for_button_to_sleep(void *pvParameters);
void do_short_press(void);
void do_long_press(void);

#endif