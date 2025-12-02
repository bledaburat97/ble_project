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

void init_current_therapy_info_manager();

void start_therapy(bool is_by_app);
void start_new_therapy(uint16_t duration);            // therapy_id’yi içeride üretmiyorsan, çağıran set_new_therapy() yapsın
void pause_therapy(void);
void pause_therapy_because_of_alert(void);
void terminate_therapy(void);
void start_or_continue_therapy(bool is_by_app);

CurrentTherapyState get_current_therapy_state(void);
uint16_t get_current_therapy_duration(void);
uint16_t get_current_therapy_id(void);
uint16_t get_current_therapy_passed_duration(void);
uint16_t get_new_therapy_id_for_new_therapy(void);
uint16_t get_passed_duration_before_last_pause(void);

bool restore_uncompleted_therapy_if_exists(void);
#endif