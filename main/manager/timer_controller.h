#ifndef TIMER_CONTROLLER_H
#define TIMER_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "../storage/log_types.h"

bool stop_therapy_timer();
bool start_therapy_timer(uint16_t duration);
bool stop_inactivity_timer();
bool start_inactivity_timer();
bool start_alert_timer();
void register_timer_end_callback(void (*callback)(NotificationType));
void init_timer_controller();

#endif