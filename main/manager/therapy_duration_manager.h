
#ifndef THERAPY_DURATION_MANAGER_H
#define THERAPY_DURATION_MANAGER_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

void clear_planned_therapy_duration();
bool try_set_planned_therapy_duration(uint16_t duration);
uint16_t get_planned_therapy_duration_s();
void clear_paused_therapy_passed_duration();
void accumulate_paused_therapy_passed_duration(uint32_t therapy_timer_passed_ms, const char *reason);
bool check_uncompleted_paused_therapy_exists();
void set_paused_therapy_passed_duration_ms(uint16_t therapy_passed_seconds);
uint16_t get_remaining_therapy_duration();
uint32_t get_paused_therapy_passed_duration_ms();

#endif