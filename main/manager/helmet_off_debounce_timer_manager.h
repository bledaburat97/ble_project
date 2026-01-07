#ifndef HELMET_OFF_DEBOUNCE_TIMER_MANAGER_H
#define HELMET_OFF_DEBOUNCE_TIMER_MANAGER_H

#include <stdbool.h>

bool stop_helmet_off_debounce_timer(void);
bool is_helmet_off_debounce_timer_running(void);
bool start_helmet_off_debounce_timer(void);

#endif