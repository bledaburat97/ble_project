#include <stdint.h>
#include "notification.h"

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
#define INACTIVITY_THRESHOLD_SECONDS 50  // Default value
#define MAX_THERAPY_APPLIED_DURATION 2040
void start_therapy_timer(uint16_t duration_bits);
void stop_therapy_timer();
void start_inactivity_timer();
void register_timer_notification_callback(void (*callback)(NotificationType));
uint16_t get_last_therapy_applied_duration();

#endif 