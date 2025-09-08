#include "timer_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "deep_sleep_manager.h"
#include "state_manager.h"
#include "temperature_alarm_control.h"
#include "timer_utils.h"
#include "storage/log_writer.h"
#include "storage/log_utils.h"
#include "esp_timer.h"

static const char *TAG = "TimerManagement";

//static int64_t last_upd_passed_dur_time = 0;
//static uint32_t last_upd_passed_dur_carry_us = 0; 

static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alert_timer = NULL;

//static uint16_t passed_timer_duration = 0;
//static uint16_t active_therapy_duration = DEFAULT_THERAPY_DURATION;
static uint16_t active_therapy_timer_duration = DEFAULT_THERAPY_DURATION;

static void (*timer_state_change_callback)(NotificationType) = NULL;
static void (*timer_end_callback)(NotificationType) = NULL;
static void (*timer_start_callback)(DeviceState) = NULL;

bool is_inactivity_timer_running()
{
    if (inactivity_timer == NULL)
    {
        ESP_LOGI(TAG, "Inactivity timer is null!");
        return false;
    }
    return true;
}

bool is_therapy_timer_running()
{
    if (therapy_timer == NULL)
    {
        ESP_LOGI(TAG, "Therapy timer is null!");
        return false;
    }
    return true;
}

bool is_alert_timer_running()
{
    if (alert_timer == NULL)
    {
        ESP_LOGI(TAG, "Alarm timer is null!");
        return false;
    }
    return true;
}

static void therapy_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        timer_end_callback(NOTIF_THERAPY_COMPLETED);
    }
}

bool stop_inactivity_timer() {
    if(stop_and_delete_timer(&inactivity_timer)){
        ESP_LOGI(TAG, "Inactivity timer is stopped.");
        return true;
    }
    else{
        ESP_LOGI(TAG, "Inactivity timer can not be stopped.");
        return false;
    }
}

bool stop_therapy_timer()
{
    if(stop_and_delete_timer(&therapy_timer)){
        ESP_LOGI(TAG, "Therapy timer is stopped.");
        return true;
    }
    else{
        ESP_LOGI(TAG, "Therapy timer can not be stopped.");
        return false;
    }
}

bool stop_alert_timer()
{
    if(stop_and_delete_timer(&alert_timer)){
        ESP_LOGI(TAG, "Alert timer is stopped.");
        return true;
    }
    else{
        ESP_LOGI(TAG, "Alert timer can not be stopped.");
        return false;
    }
}

void start_therapy_timer(uint16_t duration, NotificationType notification_type) {
    active_therapy_timer_duration = duration;
    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);
    timer_state_change_callback(notification_type);
    set_device_state(STATE_ACTIVE);
    //if(timer_start_callback) {
    //    timer_start_callback(STATE_ACTIVE);
    //}

    //last_upd_passed_dur_time = esp_timer_get_time();
    //last_upd_passed_dur_carry_us = 0;
}

static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        timer_end_callback(NOTIF_INACTIVITY_TIMER_EXPIRED);
    }
}

static void alert_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        timer_end_callback(NOTIF_ALERT_TIMER_EXPIRED);
    }
}

void register_timer_end_callback(void (*callback)(NotificationType)) {
    timer_end_callback = callback;
}

void register_timer_state_change_callback(void (*callback)(NotificationType)) {
    timer_state_change_callback = callback;
}

void start_alert_timer(int sensor_index) {
    if (!alert_timer) {
        alert_timer = create_and_start_timer(STATE_TEMPERATURE_ALERT, ALARM_THRESHOLD_SECONDS * 1000, alert_timer_expiry_callback);
        if(timer_start_callback) {
            timer_start_callback(STATE_TEMPERATURE_ALERT);
        }
    }
    set_device_state(STATE_TEMPERATURE_ALERT);
    if (timer_state_change_callback) {
        if(sensor_index == 0){
            timer_state_change_callback(TIMER_STATE_HIGH_TEMP_ALERT_1);
        }
        else if(sensor_index == 1){
            timer_state_change_callback(TIMER_STATE_HIGH_TEMP_ALERT_2);
        }

        else if(sensor_index == 2){
            timer_state_change_callback(TIMER_STATE_HIGH_TEMP_ALERT_3);
        }
    }
}

bool start_inactivity_timer() {
    if (!inactivity_timer) {
        inactivity_timer = create_and_start_timer(STATE_INACTIVE, INACTIVITY_THRESHOLD_SECONDS * 1000, inactivity_timer_expiry_callback);
        if(timer_start_callback) {
            timer_start_callback(STATE_INACTIVE);
        }
    }
    set_device_state(STATE_INACTIVE);

    ESP_LOGI(TAG, "Set state as inactive.");
    if (timer_state_change_callback) {
        timer_state_change_callback(TIMER_STATE_INACTIVE);
    }
    return true;
}

static uint16_t get_therapy_remaining_seconds_direct(void) {
    if (!therapy_timer) return 0;
    TickType_t now = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(therapy_timer);  // bir sonraki timeout tick’i
    if (expiry <= now) return 0;
    TickType_t remain_ticks = expiry - now;
    uint32_t remain_ms = remain_ticks * portTICK_PERIOD_MS;
    return (uint16_t)(remain_ms / 1000);
}

uint16_t get_therapy_passed_seconds_direct(void) {
    // Sadece AKTİF iken güvenilir: passed = total - remaining
    uint16_t total = active_therapy_timer_duration; // saniye
    uint16_t rem   = get_therapy_remaining_seconds_direct();
    return (rem >= total) ? 0 : (total - rem);
}

/*

static void update_watchdog_timeout_callback(TimerHandle_t xTimer) {
    if(get_device_state() == STATE_ACTIVE) {
        add_notification_log(PASSED_DURATION_UPDATED, get_passed_duration());
    }
}


void reset_passed_therapy_duration() {
    ESP_LOGI(TAG, "Reset passed therapy duration!");
    //passed_duration = 0;
    //active_therapy_duration = DEFAULT_THERAPY_DURATION;
    last_upd_passed_dur_time = 0;
    last_upd_passed_dur_carry_us = 0;
}


void init_timer_manager()
{
    reset_passed_therapy_duration();
}


uint16_t get_passed_timer_duration(){
    if(get_device_state() == STATE_ACTIVE) {
        update_passed_therapy_duration();
    }
    return passed_duration;
}



void update_passed_therapy_duration() {
    ESP_LOGI(TAG, "Update passed therapy duration!");

    int64_t current_time = esp_timer_get_time();
    if (last_upd_passed_dur_time == 0) {
        last_upd_passed_dur_time = current_time;
        return;
    }

    int64_t time_diff = current_time - last_upd_passed_dur_time;
    if (time_diff <= 0) {
        // Saat geri gidemez ama teoride overflow/yeniden başlat gibi durumlarda korunma:
        last_upd_passed_dur_time = current_time;
        return;
    }

    uint64_t total_time_diff_us = (uint64_t)time_diff + (uint64_t)last_upd_passed_dur_carry_us;

    uint32_t add_secs = (uint32_t)(total_time_diff_us / 1000000ULL);
    last_upd_passed_dur_carry_us = (uint32_t)(total_time_diff_us % 1000000ULL);

    if (add_secs == 0) {
        // Henüz 1 saniye dolmadı
        return;
    }

    if (add_secs > MAX_THERAPY_DURATION) {
        ESP_LOGE(TAG, "Time is miscalculated. Time diff (s): %lu", add_secs);
        last_upd_passed_dur_time = current_time;
        return;
    }

    if ((uint32_t)passed_duration + add_secs > MAX_THERAPY_DURATION) {
        ESP_LOGE(TAG, "Passed duration is miscalculated. Total (s): %lu", (uint32_t)passed_duration + add_secs);
        last_upd_passed_dur_time = current_time;
        return;
    }

    passed_duration += add_secs;
    last_upd_passed_dur_time = current_time;

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
}


*/