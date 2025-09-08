#include "current_therapy_info_manager.h"

#include <stdint.h>
#include "storage/log_writer.h"
#include "therapy_counter.h"
#include "esp_log.h"
#include "matching_message_encoder.h"
#include "timer_management.h"
#include "state_manager.h"
#include "timer_utils.h"

static const char *TAG = "CurrentTherapyInfoManager";

static CurrentTherapyState current_therapy_state = NONE;
static uint16_t current_therapy_duration = 0;
static uint16_t passed_duration_before_last_pause = 0;
static uint16_t current_therapy_id = 0;
static TimerHandle_t update_watchdog_timer = NULL;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10 * 1000; // 10 saniye

static void clear_current_therapy() {
    current_therapy_id = 0;
    current_therapy_duration = 0;
    passed_duration_before_last_pause = 0;
}

static void stop_duration_update_watchdog_timer() {
    if (update_watchdog_timer == NULL) {
        ESP_LOGD(TAG, "No watchdog to stop.");
        return;
    }
    if (xTimerIsTimerActive(update_watchdog_timer)) {
        xTimerStop(update_watchdog_timer, 0);
    }
    xTimerDelete(update_watchdog_timer, 0);
    update_watchdog_timer = NULL;
}

void pause_therapy_because_of_alert() {
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = PAUSED;
    uint16_t direct = get_therapy_passed_seconds_direct();
    uint32_t new_total = (uint32_t)passed_duration_before_last_pause + direct;
    if (new_total > current_therapy_duration) new_total = current_therapy_duration;
    passed_duration_before_last_pause = (uint16_t)new_total;
    stop_therapy_timer();
    stop_duration_update_watchdog_timer();
}

void pause_therapy() {
    if(get_device_state() != STATE_ACTIVE) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = PAUSED;
    uint16_t direct = get_therapy_passed_seconds_direct();
    uint32_t new_total = (uint32_t)passed_duration_before_last_pause + direct;
    if (new_total > current_therapy_duration) new_total = current_therapy_duration;
    passed_duration_before_last_pause = (uint16_t)new_total;
    stop_therapy_timer();
    start_inactivity_timer();
    stop_duration_update_watchdog_timer();
}

void terminate_therapy() {
    if(get_device_state() != STATE_ACTIVE) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Big error.");
        return;
    }
    current_therapy_state = NONE;
    clear_current_therapy();
    stop_therapy_timer();
    start_inactivity_timer();
    stop_duration_update_watchdog_timer();
}

void continue_therapy() {
    if(get_device_state() != STATE_INACTIVE) {
        return;
    }
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

static void update_watchdog_timeout_callback(TimerHandle_t xTimer) {
    if(get_device_state() == STATE_ACTIVE) {
        add_notification_log(PASSED_DURATION_UPDATED, get_current_therapy_passed_duration());
    }
}

uint16_t get_current_therapy_passed_duration() {
    if (get_device_state() == STATE_ACTIVE) {
        if (update_watchdog_timer == NULL) {
            update_watchdog_timer = xTimerCreate(
                "UpdateWatchdog",
                pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS),
                pdFALSE,
                NULL,
                update_watchdog_timeout_callback
            );
        }
        if (xTimerIsTimerActive(update_watchdog_timer)) {
            xTimerStop(update_watchdog_timer, 0);
        }
        xTimerStart(update_watchdog_timer, 0);
    }

    uint32_t total = (uint32_t)passed_duration_before_last_pause + get_therapy_passed_seconds_direct();
    if (total > current_therapy_duration) total = current_therapy_duration;
    return (uint16_t)total;
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
    passed_duration_before_last_pause = 0;
}

void start_new_therapy(uint16_t duration) {
    uint16_t therapy_id = 0;//get_therapy_id
    set_new_therapy(therapy_id, duration);
    start_therapy_timer(duration, TIMER_STATE_NEW_THERAPY_BY_APP);
    current_therapy_state = ACTIVE;
}

//bu metottan önce set_new_therapy kesin çağrılmış olmalı.
void start_therapy(bool is_by_app) {
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

void try_start_new_therapy_by_activation(uint16_t duration) {
    if(get_device_state() == STATE_ACTIVE) {
        if(is_therapy_timer_running()) {
            ESP_LOGI(TAG, "On activate when state active");
            stop_therapy_timer();
            set_device_state(STATE_IDLE);
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state active but therapy timer is not running.");
        }
    } else if(get_device_state() == STATE_INACTIVE && get_helmet_state()) {
        if(is_inactivity_timer_running()) {
            ESP_LOGI(TAG, "On activate when state inactive");
            stop_inactivity_timer();
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state inactive but inactivity timer is not running.");
        }
    }
}

void set_inactivity_after_alert_expires() {
    if(is_alert_timer_running()) {
        stop_alert_timer();
        start_inactivity_timer();
    }
    else {
        //ERROR
    }
}

void turn_off_device_because_of_inactivity() {
    if (!stop_inactivity_timer()) {
        return;
    }
    set_device_state(STATE_IDLE);
}

/*
static void try_set_uncompleted_therapy_as_current_therapy(uint16_t last_saved_therapy_id) {
    ReadTherapyInfo therapy_info;
    if(read_therapy_info(therapy_id, &therapy_info)) {
        if(therapy_info.is_over) {
            current_therapy_state = NONE;
            return;
        }
        active_therapy_duration = therapy_info.therapy_duration;
        memcpy(active_brightness, therapy_info.brightness, sizeof(active_brightness));
        passed_duration = therapy_info.passed_duration;
        current_therapy_state = PAUSED;
    }
}
*/
void init_current_therapy_info_manager() {
    //register_timer_start_callback(on_timer_start);
    /*
    uint16_t last_saved_therapy_id = read_therapy_count();
    if(last_saved_therapy_id == 0) {
        ESP_LOGI(TAG, "There is not any saved therapy.");
    }
    else {
        try_set_uncompleted_therapy_as_current_therapy(last_saved_therapy_id);
    }
    */
}





