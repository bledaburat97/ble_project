#include <stdint.h>
#include "storage/log_types.h"
#include "state_manager.h"

#ifndef TIMER_MANAGEMENT_H
#define TIMER_MANAGEMENT_H
#define INACTIVITY_THRESHOLD_SECONDS 500
#define ALARM_THRESHOLD_SECONDS 60
#define DEFAULT_THERAPY_DURATION 1500
#define MAX_THERAPY_DURATION 3600

bool start_inactivity_timer(void);
void start_alert_timer(int sensor_index);
void start_therapy_timer(uint16_t duration, NotificationType notif);
bool stop_inactivity_timer(void);
bool stop_alert_timer(void);
bool stop_therapy_timer(void);

bool is_inactivity_timer_running(void);
bool is_alert_timer_running(void);
bool is_therapy_timer_running(void);

uint16_t get_therapy_passed_seconds_direct(void); // Sadece ACTIVE aralığı
void register_timer_end_callback(void (*)(NotificationType));
void register_timer_state_change_callback(void (*)(NotificationType));
void set_passed_duration_before_last_pause(uint16_t duration);
uint16_t get_passed_duration_before_last_pause();
uint16_t get_current_therapy_passed_duration(void);

/*
void start_new_therapy(uint16_t duration);
void start_therapy(bool is_by_app);
void register_timer_start_callback(void (*callback)(DeviceState));
void init_timer_manager();
uint16_t get_passed_duration();
void update_passed_therapy_duration();
void reset_passed_therapy_duration();
*/
#endif 