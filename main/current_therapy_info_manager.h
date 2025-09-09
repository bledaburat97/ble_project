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

void set_new_therapy(uint16_t therapy_id, uint16_t total_duration);
void start_new_therapy(uint16_t duration);            // therapy_id’yi içeride üretmiyorsan, çağıran set_new_therapy() yapsın
void start_therapy(bool is_by_app);                    // resume/continue
void pause_therapy(void);
void pause_therapy_because_of_alert(void);
void terminate_therapy(void);
void continue_therapy(void);

CurrentTherapyState get_current_therapy_state(void);
uint16_t get_current_therapy_duration(void);
uint16_t get_current_therapy_id(void);

#endif