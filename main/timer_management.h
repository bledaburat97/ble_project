#include <stdint.h>
#include "notification.h"

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
#define INACTIVITY_THRESHOLD_SECONDS 50  // Default value
void start_therapy_timer(uint32_t duration);
void get_last_therapy_data(uint8_t *buffer);
void stop_therapy_timer();
void start_inactivity_timer();
void restart_inactivity_timer();
void register_timer_notification_callback(void (*callback)(NotificationType));
#endif 


