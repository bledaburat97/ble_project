#include "timer_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "time.h"

#include "storage_management.h"
#include "deep_sleep_manager.h"
#include "state_manager.h"
#include "temperature_alarm_control.h"
#include "timer_utils.h"
#include "log_writer.h"
#include "log_utils.h"

static const char *TAG = "TimerManagement";

static time_t last_updated_passed_duration_time = 0;
static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alarm_timer = NULL;

static uint16_t passed_duration = 0;
static uint16_t active_therapy_duration = DEFAULT_THERAPY_DURATION;

static void (*timer_start_callback)(NotificationType) = NULL;
static void (*timer_end_callback)(NotificationType) = NULL;

static bool is_inactivity_timer_running()
{
    if (inactivity_timer == NULL)
    {
        ESP_LOGI(TAG, "Inactivity timer is null!");
        return false;
    }
    return true;
}

static bool is_therapy_timer_running()
{
    if (therapy_timer == NULL)
    {
        ESP_LOGI(TAG, "Therapy timer is null!");
        return false;
    }
    return true;
}

static bool is_alarm_timer_running()
{
    if (alarm_timer == NULL)
    {
        ESP_LOGI(TAG, "Alarm timer is null!");
        return false;
    }
    return true;
}

static void therapy_timer_expiry_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Therapy timer expired!");
    if (timer_end_callback) {
        timer_end_callback(THERAPY_COMPLETED);
    }
    start_inactivity_timer();
    reset_passed_therapy_duration();
}

static void stop_inactivity_timer() {
    if(stop_and_delete_timer(&inactivity_timer)){
        ESP_LOGI(TAG, "Inactivity timer is stopped.");
        set_device_state(STATE_IDLE);
    }
    else{
        ESP_LOGI(TAG, "Inactivity timer can not be stopped.");
    }
}

static void stop_therapy_timer()
{
    if(stop_and_delete_timer(&therapy_timer)){
        ESP_LOGI(TAG, "Therapy timer is stopped.");
        set_device_state(STATE_IDLE);
    }
    else{
        ESP_LOGI(TAG, "Therapy timer can not be stopped.");
    }
}

static void start_therapy_timer(uint16_t duration) {
    if(is_alarm_timer_running()) {
        ESP_LOGE(TAG, "Alarm timer wasn't stopped.");
        return;
    }

    if(is_inactivity_timer_running()) {
        stop_inactivity_timer();
    }

    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);
    
    set_device_state(STATE_ACTIVE);

    last_updated_passed_duration_time = time(NULL);
}

void start_new_therapy(uint16_t duration) {
    if(is_therapy_timer_running()) {
        stop_therapy_timer();
    }
    active_therapy_duration = duration;

    start_therapy_timer(duration);
}

void start_therapy(bool is_by_app) {
    if(is_therapy_timer_running()) {
        ESP_LOGE(TAG, "There shouldn't have been an active therapy.");
        return;
    }
    if(passed_duration >= active_therapy_duration) {
        ESP_LOGE(TAG, "Passed duration: %u is more than therapy duration: %u", passed_duration, active_therapy_duration);
        return;
    }
    uint16_t duration = active_therapy_duration - passed_duration; //TODO: passed_duration'ın 5 dk cihaz çalıştıtılmazsa sıfırlanması lazım.
    start_therapy_timer(duration);
    if(passed_duration == 0) {
        if (timer_start_callback) {
            if(!is_by_app) {
                timer_start_callback(THERAPY_STARTED_BY_BUTTON);
            }
            else{
                ESP_LOGE(TAG, "App does not start default therapy.");
            }
        }
    }
    else {
        if (timer_start_callback) {
            if(!is_by_app) {
                timer_start_callback(THERAPY_CONTINUED_BY_BUTTON);
            }
            else{
                timer_start_callback(THERAPY_CONTINUED_BY_APP);
            }
        }
    }
}

static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Inactivity timer expired!");
    stop_inactivity_timer();
    if (timer_end_callback) {
        timer_end_callback(INACTIVITY_TIMER_EXPIRED);
    }
    enter_deep_sleep();
}

static void alert_timer_expiry_callback(TimerHandle_t xTimer) {
    set_device_state(STATE_START);
    if (timer_end_callback) {
        timer_end_callback(ALERT_TIMER_EXPIRED);
    }
}

void register_timer_end_callback(void (*callback)(NotificationType)) {
    timer_end_callback = callback;
}

void register_timer_start_callback(void (*callback)(NotificationType)) {
    timer_start_callback = callback;
}

void start_alert_timer(int sensor_index) {
    if (!alarm_timer) {
        alarm_timer = create_and_start_timer(STATE_TEMPERATURE_ALERT, ALARM_THRESHOLD_SECONDS * 1000, alert_timer_expiry_callback);
    }
    set_device_state(STATE_TEMPERATURE_ALERT);
    if (timer_start_callback) {
        if(sensor_index == 0){
            timer_start_callback(HIGH_TEMP_ALERT_1);
        }
        else if(sensor_index == 1){
            timer_start_callback(HIGH_TEMP_ALERT_2);
        }
        else if(sensor_index == 2){
            timer_start_callback(HIGH_TEMP_ALERT_3);
        }
    }
}

void start_inactivity_timer() {
    if(is_alarm_timer_running()) {
        ESP_LOGE(TAG, "Alarm timer wasn't stopped.");
        return;
    }

    if(is_therapy_timer_running()) {
        stop_therapy_timer();
    }

    if (!inactivity_timer) {
        inactivity_timer = create_and_start_timer(STATE_INACTIVITY, INACTIVITY_THRESHOLD_SECONDS * 1000, inactivity_timer_expiry_callback);
    }
    set_device_state(STATE_INACTIVITY);
    if (timer_start_callback) {
        timer_start_callback(INACTIVITY_TIMER_STARTED);
    }
}
    
static void on_helmet_state_changed(bool helmet_state) {
    if (!helmet_state) {
        if (get_device_state() == STATE_ACTIVE) {
            start_inactivity_timer();
            update_passed_therapy_duration();
        }
    }
}

void reset_passed_therapy_duration() {
    ESP_LOGI(TAG, "Reset passed therapy duration!");
    passed_duration = 0;
    active_therapy_duration = DEFAULT_THERAPY_DURATION;
    last_updated_passed_duration_time = 0;
}

void update_passed_therapy_duration() {
    ESP_LOGI(TAG, "Update passed therapy duration!");

    time_t current_time;
    time(&current_time);

    uint16_t time_diff = (uint16_t)(current_time - last_updated_passed_duration_time);

    if (time_diff > MAX_THERAPY_DURATION) {
        ESP_LOGE(TAG, "Time is miscalculated. Time diff: %u", time_diff);
        return;
    }

    if (passed_duration + time_diff > MAX_THERAPY_DURATION) {
        ESP_LOGE(TAG, "Passed duration is miscalculated. Total: %u", passed_duration + time_diff);
        return;
    }

    passed_duration += time_diff;
    last_updated_passed_duration_time = current_time;
}

void init_timer_manager()
{
    reset_passed_therapy_duration();
    register_helmet_state_change_callback(on_helmet_state_changed);
}

uint16_t get_passed_duration(){
    if(get_device_state() == STATE_ACTIVE) {
        update_passed_therapy_duration();
    }
    return passed_duration;
}