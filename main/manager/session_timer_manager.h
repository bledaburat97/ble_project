#ifndef SESSION_TIMER_MANAGER_H
#define SESSION_TIMER_MANAGER_H

#include <stdint.h>

uint16_t get_session_passed_seconds(void);
void clear_session_clock(void);
void reset_session_clock(int64_t passed_time);

#endif