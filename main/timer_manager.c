#include "timer_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "deep_sleep_manager.h"
#include "state_manager.h"
#include "temperature_alert_controller.h"
#include "timer_utils.h"
#include "storage/log_writer.h"
#include "storage/log_utils.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "device_configuration.h"

static const char *TAG = "TimerManager";

static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alert_timer = NULL;

static uint16_t active_therapy_timer_duration = DEFAULT_THERAPY_DURATION;

static void (*timer_state_change_callback)(NotificationType) = NULL;
static void (*timer_end_callback)(NotificationType) = NULL;
static TimerHandle_t update_watchdog_timer = NULL;
static const uint32_t WATCHDOG_TIMEOUT_MS = 10 * 1000; // 10 saniye

static int64_t session_start_us = -1; // -1: aktif oturum yok

static inline bool is_session_running(void) { return session_start_us >= 0; }

typedef enum {
    EVT_WATCHDOG_TICK,
    EVT_THERAPY_COMPLETED,
    EVT_ALERT_EXPIRED,
    EVT_INACTIVITY_EXPIRED
} SystemEvent;

QueueHandle_t g_systemEvtQ = NULL;

void clear_session_clock(void) {
    session_start_us = -1;
}

void reset_session_clock(void) { 
    session_start_us = esp_timer_get_time();
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
        SystemEvent ev = EVT_WATCHDOG_TICK;
        (void)xQueueSend(g_systemEvtQ, &ev, 0);
        (void)xTimerReset(update_watchdog_timer, 0);
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
    SystemEvent ev = EVT_THERAPY_COMPLETED;
    (void)xQueueSend(g_systemEvtQ, &ev, 0);
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
    }
    set_device_state(STATE_ACTIVE);

    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);

    start_duration_update_watchdog_timer();

    if (timer_state_change_callback) {
        timer_state_change_callback(notification_type);
    }
}

static void alert_timer_expiry_callback(TimerHandle_t xTimer) {
    SystemEvent ev = EVT_ALERT_EXPIRED;
    (void)xQueueSend(g_systemEvtQ, &ev, 0);
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
    SystemEvent ev = EVT_INACTIVITY_EXPIRED;
    (void)xQueueSend(g_systemEvtQ, &ev, 0);
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

uint32_t get_therapy_remaining_ms(void) {
    if (!therapy_timer) return 0;

    if (xTimerIsTimerActive(therapy_timer) != pdTRUE) {
        // HER ZAMAN ms döndür
        return (uint32_t)active_therapy_timer_duration * 1000u;
    }

    TickType_t now    = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(therapy_timer);
    if (expiry <= now) return 0;

    TickType_t remain_ticks = expiry - now;
#ifdef pdTICKS_TO_MS
    uint32_t ms = (uint32_t)pdTICKS_TO_MS(remain_ticks);
#else
    uint32_t ms = (uint32_t)remain_ticks * (uint32_t)portTICK_PERIOD_MS;
#endif

    uint32_t nominal = (uint32_t)active_therapy_timer_duration * 1000u;

    if (ms + 1000u > nominal) {
        return nominal;
    }

    // aşırı ölçüm hatası durumunda yukarı clamp
    if (ms > nominal) return nominal;

    return ms;
}

uint32_t get_therapy_passed_ms_direct(void) {
    uint32_t total_ms   = (uint32_t)active_therapy_timer_duration * 1000u;
    uint32_t remain_ms = get_therapy_remaining_ms();
    if (remain_ms >= total_ms) return 0;
    return total_ms - remain_ms;
}

uint16_t get_therapy_remaining_seconds(void) {
    uint32_t ms = get_therapy_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

uint16_t get_therapy_passed_seconds_direct(void) {
    uint32_t plan_ms = (uint32_t)active_therapy_timer_duration * 1000u;
    uint32_t rem_ms  = get_therapy_remaining_ms();
    uint32_t passed  = (rem_ms >= plan_ms) ? 0u : (plan_ms - rem_ms);
    return (uint16_t)(passed / 1000u);
}

uint16_t get_inactivity_duration(void) {
    return INACTIVITY_THRESHOLD_SECONDS;
}

uint32_t get_inactivity_remaining_ms(void) {
    if (!inactivity_timer) return 0;

    if (xTimerIsTimerActive(inactivity_timer) != pdTRUE) {
         return (uint32_t)INACTIVITY_THRESHOLD_SECONDS * 1000u;
    }
    TickType_t now = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(inactivity_timer);
    if (expiry <= now) return 0;

    TickType_t remain = expiry - now;
    TickType_t nominal = pdMS_TO_TICKS((uint32_t)INACTIVITY_THRESHOLD_SECONDS * 1000u);

    if (remain > (nominal + pdMS_TO_TICKS(1000u)) * 2)
        return INACTIVITY_THRESHOLD_SECONDS;

    #ifdef pdTICKS_TO_MS
    return (uint32_t)pdTICKS_TO_MS(remain);
    #else
    return (uint32_t)remain * (uint32_t)portTICK_PERIOD_MS;
    #endif
}

uint16_t get_inactivity_remaining_seconds(void) {
    uint32_t ms = get_inactivity_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

uint16_t get_alert_duration(void) {
    return ALERT_THRESHOLD_SECONDS;
}

uint32_t get_alert_remaining_ms(void) {
    if (!alert_timer) return 0;
    if (xTimerIsTimerActive(alert_timer) != pdTRUE) {
        return (uint32_t)ALERT_THRESHOLD_SECONDS * 1000u;
    }

    TickType_t now = xTaskGetTickCount();
    TickType_t expiry = xTimerGetExpiryTime(alert_timer);
    if (expiry <= now) return 0;

    TickType_t remain = expiry - now;
    TickType_t nominal = pdMS_TO_TICKS((uint32_t)ALERT_THRESHOLD_SECONDS * 1000u);
    if (remain > (nominal + pdMS_TO_TICKS(1000u)) * 2) {
        return (uint32_t)ALERT_THRESHOLD_SECONDS * 1000u;
    }

    #ifdef pdTICKS_TO_MS
    return (uint32_t)pdTICKS_TO_MS(remain);
    #else
    return (uint32_t)remain * (uint32_t)portTICK_PERIOD_MS;
    #endif
}

uint16_t get_alert_remaining_seconds(void) {
    uint32_t ms = get_alert_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

void restart_duration_update_watchdog_timer(void) {
    if (update_watchdog_timer) {
        xTimerStop(update_watchdog_timer, 0);
        xTimerStart(update_watchdog_timer, 0);
    }
}

uint16_t get_session_passed_seconds(void) {
    if (!is_session_running()) {
        ESP_LOGI(TAG, "Session is not open");
        return 0;
    }

    int64_t now_us = esp_timer_get_time();

    int64_t diff = now_us - session_start_us;
    if (diff < 0) diff = 0;
    return (uint16_t)(diff / 1000000LL); // saniye
}

static void ManagerTask(void *arg) {
    SystemEvent ev;
    for (;;) {
        if (xQueueReceive(g_systemEvtQ, &ev, portMAX_DELAY)) {
            switch (ev) {
            case EVT_WATCHDOG_TICK:
                add_notification_log(PASSED_DURATION_UPDATED, get_session_passed_seconds());
                vTaskDelay(1);
                break;
            case EVT_THERAPY_COMPLETED:
                stop_therapy_timer();
                if (timer_end_callback) timer_end_callback(NOTIF_THERAPY_COMPLETED);
                vTaskDelay(1);
                break;
            case EVT_ALERT_EXPIRED:
                stop_alert_timer();
                ESP_LOGE(TAG, "CREATE ALERT EXPIRED");
                if (timer_end_callback) timer_end_callback(NOTIF_ALERT_TIMER_EXPIRED);
                vTaskDelay(1);
                break;
            case EVT_INACTIVITY_EXPIRED:
                stop_inactivity_timer();
                if (timer_end_callback) timer_end_callback(NOTIF_INACTIVITY_TIMER_EXPIRED);
                vTaskDelay(1);
                break;
            }
        }
    }
}

void init_timer_manager() {
    g_systemEvtQ = xQueueCreate(16, sizeof(SystemEvent));
    xTaskCreate(ManagerTask, "ManagerTask", 4096, NULL, 5, NULL);
}