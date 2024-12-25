#include <stdint.h>

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
void start_therapy_timer(uint32_t duration);
void get_last_therapy_data(uint8_t *buffer);
void stop_therapy_timer();

#endif 


