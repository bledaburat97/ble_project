#include "current_therapy_info_manager.h"

#include <stdint.h>
#include "esp_log.h"
#include "timer_management.h"

static const char *TAG = "CurrentTherapyInfoManager";

static CurrentTherapyState current_therapy_state = NONE;
static uint16_t current_therapy_duration = 0;
static uint16_t current_therapy_id = 0;

static void clear_current_therapy() {
    current_therapy_id = 0;
    current_therapy_duration = 0;
    set_passed_duration_before_last_pause(0);
}

void pause_therapy_because_of_alert() {
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = PAUSED;
    uint16_t direct = get_therapy_passed_seconds_direct();
    uint32_t new_total = (uint32_t)get_passed_duration_before_last_pause() + direct;
    if (new_total > current_therapy_duration) new_total = current_therapy_duration;
    set_passed_duration_before_last_pause((uint16_t)new_total);
    stop_therapy_timer();
}

void pause_therapy() {
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = PAUSED;
    uint16_t direct = get_therapy_passed_seconds_direct();
    uint32_t new_total = (uint32_t)get_passed_duration_before_last_pause() + direct;
    if (new_total > current_therapy_duration) new_total = current_therapy_duration;
    set_passed_duration_before_last_pause((uint16_t)new_total);
    stop_therapy_timer();
}

void terminate_therapy() {
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    stop_therapy_timer();
    current_therapy_state = NONE;
    clear_current_therapy();
}

void continue_therapy() {
    if(!is_inactivity_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = ACTIVE;
    stop_inactivity_timer();
    start_therapy(true);
}

CurrentTherapyState get_current_therapy_state() {
    return current_therapy_state;
}

uint16_t get_current_therapy_duration() {
    return current_therapy_duration;
}

uint16_t get_current_therapy_id() {
    return current_therapy_id;
}

void set_new_therapy(uint16_t therapy_id, uint16_t total_duration) {
    current_therapy_id = therapy_id;
    current_therapy_duration = total_duration;
    set_passed_duration_before_last_pause(0);
}

void start_new_therapy(uint16_t duration) {
    uint16_t therapy_id = 0;//get_therapy_id
    set_new_therapy(therapy_id, duration);
    start_therapy_timer(duration, TIMER_STATE_NEW_THERAPY_BY_APP);
    current_therapy_state = ACTIVE;
}

//bu metottan önce set_new_therapy kesin çağrılmış olmalı.
void start_therapy(bool is_by_app) {
    uint16_t passed_duration_before_last_pause = get_passed_duration_before_last_pause();
    if(passed_duration_before_last_pause >= current_therapy_duration) {
        ESP_LOGE(TAG, "Passed duration: %u is more than therapy duration: %u", passed_duration_before_last_pause, current_therapy_duration);
        return;
    }
    if(passed_duration_before_last_pause == 0) {
        if(!is_by_app) {
            start_therapy_timer(current_therapy_duration, TIMER_STATE_NEW_THERAPY_BY_BUTTON);
            current_therapy_state = ACTIVE;
        }
        else{
            ESP_LOGE(TAG, "App does not start default therapy.");
        }
    }
    else {
        current_therapy_state = ACTIVE;

        if(!is_by_app) {
            start_therapy_timer(current_therapy_duration - passed_duration_before_last_pause, TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON);
        }
        else{
            start_therapy_timer(current_therapy_duration - passed_duration_before_last_pause, TIMER_STATE_CONTINUE_THERAPY_BY_APP);
        }
    }
}


void init_current_therapy_info_manager() {
    clear_current_therapy();
}





