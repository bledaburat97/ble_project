#ifndef TIMER_INFO_GETTER_H
#define TIMER_INFO_GETTER_H

#include <stdint.h>

uint32_t get_therapy_timer_passed_ms(void);
uint16_t get_inactivity_timer_remaining_s(void);
uint16_t get_therapy_timer_remaining_s(void);
uint16_t get_alert_timer_remaining_s(void);
void restart_duration_update_watchdog_timer(void);
void register_passed_duration_update(void (*callback)());

#endif