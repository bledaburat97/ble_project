#include <stdint.h>

#ifndef BBOT_BUTTON_CONTROL_H
#define BBOT_BUTTON_CONTROL_H

void monitor_boot_button_task(void *arg);
void initialize_boot_button_gpio();

#endif 