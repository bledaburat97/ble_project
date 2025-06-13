#include <stdint.h>
#include "log_types.h"

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
#define INACTIVITY_THRESHOLD_SECONDS 500
#define ALARM_THRESHOLD_SECONDS 60
#define DEFAULT_THERAPY_DURATION 1500
#define MAX_THERAPY_DURATION 3600

void start_new_therapy(uint16_t duration);
void start_therapy(bool is_by_app);
void start_inactivity_timer();
void start_alert_timer(int sensor_index);
void register_timer_start_callback(void (*callback)(NotificationType));
void register_timer_end_callback(void (*callback)(NotificationType));
void init_timer_manager();
uint16_t get_passed_duration();
void update_passed_therapy_duration();
void reset_passed_therapy_duration();
#endif 