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
static TimerHandle_t update_watchdog_timer = NULL;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10 * 1000; // 10 saniye
static uint16_t passed_duration_before_last_pause = 0;

static int64_t session_start_us = -1; // -1: aktif oturum yok

static inline bool is_session_running(void) { return session_start_us >= 0; }

void reset_session_clock(void) { 
    session_start_us = esp_timer_get_time();
}

void set_passed_duration_before_last_pause(uint16_t duration) {
    passed_duration_before_last_pause = duration;
}

uint16_t get_passed_duration_before_last_pause() {
    return passed_duration_before_last_pause;
}

static bool is_timer_running(TimerHandle_t h) {
    return (h != NULL) && (xTimerIsTimerActive(h) == pdTRUE);
}

bool is_inactivity_timer_running()
{
    return is_timer_running(inactivity_timer);
}

bool is_therapy_timer_running()
{
    return is_timer_running(therapy_timer);
}

bool is_alert_timer_running()
{
    return is_timer_running(alert_timer);
}

static void update_watchdog_timeout_callback(TimerHandle_t xTimer) {
    if(get_device_state() == STATE_ACTIVE) {
        add_notification_log(PASSED_DURATION_UPDATED, get_session_passed_seconds());
        xTimerStart(update_watchdog_timer, 0);
    }
}

static void start_duration_update_watchdog_timer(void) {
    if (!update_watchdog_timer) {
        update_watchdog_timer = xTimerCreate("UpdateWatchdog",
            pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS), pdFALSE, NULL, update_watchdog_timeout_callback);
    }
    if (xTimerIsTimerActive(update_watchdog_timer)) {
        xTimerStop(update_watchdog_timer, 0);
    }
    xTimerStart(update_watchdog_timer, 0);
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

static void therapy_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        stop_therapy_timer();
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
        stop_duration_update_watchdog_timer();
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
    ESP_LOGI(TAG, "Start therapy timer with: %u", duration);
    active_therapy_timer_duration = duration;
    if(therapy_timer != NULL) {
        stop_therapy_timer();
        ESP_LOGE(TAG, "Therapy timer should have stopped.");
    }
    reset_session_clock();
    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);
    if (timer_state_change_callback) {
        timer_state_change_callback(notification_type);
    }
    set_device_state(STATE_ACTIVE);
    start_duration_update_watchdog_timer();
}

static void alert_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        stop_alert_timer();
        ESP_LOGE(TAG, "CREATE ALERT EXPIRED");
        timer_end_callback(NOTIF_ALERT_TIMER_EXPIRED);
    }
}

void start_alert_timer(int sensor_index) {
    if (!alert_timer) {
        ESP_LOGE(TAG, "CREATE ALERT TIMER");
        alert_timer = create_and_start_timer(STATE_TEMPERATURE_ALERT, ALERT_THRESHOLD_SECONDS * 1000, alert_timer_expiry_callback);
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

static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    if (timer_end_callback) {
        stop_inactivity_timer();
        timer_end_callback(NOTIF_INACTIVITY_TIMER_EXPIRED);
    }
}

bool start_inactivity_timer() {
    if (!inactivity_timer) {
        inactivity_timer = create_and_start_timer(STATE_INACTIVE, INACTIVITY_THRESHOLD_SECONDS * 1000, inactivity_timer_expiry_callback);
    }
    set_device_state(STATE_INACTIVE);

    ESP_LOGI(TAG, "Set state as inactive.");

    if (timer_state_change_callback) {
        timer_state_change_callback(TIMER_STATE_INACTIVE);
    }
    return true;
}

void register_timer_end_callback(void (*callback)(NotificationType)) {
    timer_end_callback = callback;
}

void register_timer_state_change_callback(void (*callback)(NotificationType)) {
    timer_state_change_callback = callback;
}


uint16_t get_therapy_remaining_seconds(void) {
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
    uint16_t rem = get_therapy_remaining_seconds();
    return (rem >= total) ? 0 : (total - rem);
}

uint16_t get_inactivity_duration(void) {
    return INACTIVITY_THRESHOLD_SECONDS;
}

uint16_t get_inactivity_remaining_seconds(void) {
    if (!inactivity_timer) return 0;
    TickType_t now = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(inactivity_timer);
    if (expiry <= now) return 0;
    TickType_t remain_ticks = expiry - now;
    uint32_t remain_ms = remain_ticks * portTICK_PERIOD_MS;
    return (uint16_t)(remain_ms / 1000);
}

uint16_t get_alert_duration(void) {
    return ALERT_THRESHOLD_SECONDS;
}

uint16_t get_alert_remaining_seconds(void) {
    if (!alert_timer) return 0;
    TickType_t now = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(alert_timer);
    if (expiry <= now) return 0;
    TickType_t remain_ticks = expiry - now;
    uint32_t remain_ms = remain_ticks * portTICK_PERIOD_MS;
    return (uint16_t)(remain_ms / 1000);
}

void restart_duration_update_watchdog_timer(void) {
    if (update_watchdog_timer) {
        xTimerStop(update_watchdog_timer, 0);
        xTimerStart(update_watchdog_timer, 0);
    }
}

uint16_t get_session_passed_seconds(void) {
    if (!is_session_running()) return 0;
    int64_t diff = esp_timer_get_time() - session_start_us; // us
    if (diff < 0) diff = 0;
    return (uint16_t)(diff / 1000000LL); // saniye
}