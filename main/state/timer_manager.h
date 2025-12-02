#include <stdint.h>
#include "storage/log_types.h"
#include "state_manager.h"

#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

void init_timer_manager();

bool start_inactivity_timer(void);
void start_alert_timer(int sensor_index);
void start_therapy_timer(uint16_t duration, NotificationType notif);
bool stop_inactivity_timer(void);
bool stop_alert_timer(void);
bool stop_therapy_timer(void);

bool is_inactivity_timer_running(void);
bool is_alert_timer_running(void);
bool is_therapy_timer_running(void);

void register_timer_end_callback(void (*)(NotificationType));
void register_timer_state_change_callback(void (*)(NotificationType));
uint16_t get_therapy_remaining_seconds(void);
uint16_t get_inactivity_duration(void);
uint16_t get_inactivity_remaining_seconds(void);
uint16_t get_alert_duration(void);
uint16_t get_alert_remaining_seconds(void);
void restart_duration_update_watchdog_timer(void);
uint16_t get_session_passed_seconds(void);
void reset_session_clock(int64_t passed_time);
void clear_session_clock(void);
uint32_t get_therapy_remaining_ms(void);
uint32_t get_therapy_passed_ms_direct(void);
uint16_t get_active_therapy_duration(void);
void register_passed_duration_update(void (*callback)());
#endif 