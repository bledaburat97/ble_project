#include <stdint.h>
#include "notification.h"

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
#define INACTIVITY_THRESHOLD_SECONDS 500
#define ALARM_THRESHOLD_SECONDS 60
#define MAX_THERAPY_APPLIED_DURATION 2040
#define PERIODIC_SAVING_INTERVAL 1000
#define DEFAULT_THERAPY_DURATION 1500
void set_and_start_therapy_timer(uint16_t duration_bits);
void set_and_start_default_therapy_timer();
void start_inactivity_timer();
void start_alarm_timer();
void register_timer_notification_callback(void (*callback)(NotificationType));
uint16_t get_last_therapy_applied_duration();
void init_timer_manager();
#endif 