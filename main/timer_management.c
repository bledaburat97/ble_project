#include "timer_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "time.h"

#include "deep_sleep_manager.h"
#include "state_manager.h"
#include "temperature_alarm_control.h"
#include "timer_utils.h"
#include "storage/log_writer.h"
#include "storage/log_utils.h"

static const char *TAG = "TimerManagement";

static time_t last_updated_passed_duration_time = 0;
static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alert_timer = NULL;

static uint16_t passed_duration = 0;
static uint16_t active_therapy_duration = DEFAULT_THERAPY_DURATION;

static void (*timer_state_change_callback)(NotificationType) = NULL;
static void (*timer_end_callback)(NotificationType) = NULL;
static void (*timer_start_callback)(DeviceState) = NULL;
static TimerHandle_t update_watchdog_timer = NULL;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10 * 1000; // 10 saniye

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

static void start_therapy_timer(uint16_t duration) {
    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);
    if(timer_start_callback) {
        timer_start_callback(STATE_ACTIVE);
    }

    last_updated_passed_duration_time = time(NULL);
}

static void stop_duration_update_watchdog_timer() {
    xTimerStop(update_watchdog_timer, 0);
    xTimerDelete(update_watchdog_timer, 0);
    update_watchdog_timer = NULL;
}

void start_new_therapy(uint16_t duration) {
    active_therapy_duration = duration;
    start_therapy_timer(duration);
}

void start_therapy(bool is_by_app) {
    if(passed_duration >= active_therapy_duration) {
        ESP_LOGE(TAG, "Passed duration: %u is more than therapy duration: %u", passed_duration, active_therapy_duration);
        return;
    }
    uint16_t duration = active_therapy_duration - passed_duration; //TODO: passed_duration'ın 5 dk cihaz çalıştıtılmazsa sıfırlanması lazım.
    start_therapy_timer(duration);
    if(passed_duration == 0) {
        if (timer_state_change_callback) {
            if(!is_by_app) {
                timer_state_change_callback(TIMER_STATE_NEW_THERAPY_BY_BUTTON);
            }
            else{
                ESP_LOGE(TAG, "App does not start default therapy.");
            }
        }
    }
    else {
        if (timer_state_change_callback) {
            if(!is_by_app) {
                timer_state_change_callback(TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON);
            }
            else{
                timer_state_change_callback(TIMER_STATE_CONTINUE_THERAPY_BY_APP);
            }
        }
    }
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

void register_timer_start_callback(void (*callback)(DeviceState)) {
    timer_start_callback = callback;
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
    stop_duration_update_watchdog_timer();
}

bool start_inactivity_timer() {
    if (!inactivity_timer) {
        inactivity_timer = create_and_start_timer(STATE_INACTIVE, INACTIVITY_THRESHOLD_SECONDS * 1000, inactivity_timer_expiry_callback);
        if(timer_start_callback) {
            timer_start_callback(STATE_INACTIVE);
        }
    }
    ESP_LOGI(TAG, "Set state as inactive.");
    if (timer_state_change_callback) {
        timer_state_change_callback(TIMER_STATE_INACTIVE);
    }
    stop_duration_update_watchdog_timer();
    return true;
}

void reset_passed_therapy_duration() {
    ESP_LOGI(TAG, "Reset passed therapy duration!");
    passed_duration = 0;
    active_therapy_duration = DEFAULT_THERAPY_DURATION;
    last_updated_passed_duration_time = 0;
}

static void update_watchdog_timeout_callback(TimerHandle_t xTimer) {
    if(get_device_state() == STATE_ACTIVE) {
        add_notification_log(PASSED_DURATION_UPDATED, get_passed_duration());
    }
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

    if(get_device_state() == STATE_ACTIVE) {
        if (update_watchdog_timer == NULL) {
            update_watchdog_timer = xTimerCreate(
                "UpdateWatchdog",
                pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS),
                pdFALSE,
                NULL,
                update_watchdog_timeout_callback
            );
        }

        // Timer zaten var mı? Yeniden başlat
        if (xTimerIsTimerActive(update_watchdog_timer)) {
            xTimerStop(update_watchdog_timer, 0);
        }

        xTimerStart(update_watchdog_timer, 0);
    }
}

void init_timer_manager()
{
    reset_passed_therapy_duration();
}

uint16_t get_passed_duration(){
    if(get_device_state() == STATE_ACTIVE) {
        update_passed_therapy_duration();
    }
    return passed_duration;
}