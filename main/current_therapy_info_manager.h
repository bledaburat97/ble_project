#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifndef CURRENT_THERAPY_INFO_MANAGER_H
#define CURRENT_THERAPY_INFO_MANAGER_H


typedef enum {
    ACTIVE = 0,
    PAUSED,
    NONE,
} CurrentTherapyState;

void pause_therapy_because_of_alert();
void pause_therapy();
void terminate_therapy();
void continue_therapy();
CurrentTherapyState get_current_therapy_state();
uint16_t get_current_therapy_passed_duration();
uint16_t get_current_therapy_duration();
uint16_t get_current_therapy_id();
void set_new_therapy(uint16_t therapy_id, uint16_t total_duration);
void start_new_therapy(uint16_t duration);
void start_therapy(bool is_by_app);
void init_current_therapy_info_manager();
void try_start_new_therapy_by_activation(uint16_t duration);
void set_inactivity_after_alert_expires();
void turn_off_device_because_of_inactivity();

#endif