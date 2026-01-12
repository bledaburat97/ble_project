#include "timer_controller.h"
#include "timer_info_getter.h"

#include "../storage/log_utils.h"

#include "../i2c/temperature/temperature_alert_controller.h"
#include "../device_configuration.h"

#include "../helper/timer_utils.h"

#include "../ble/include/ble_controller.h"

#include "../buzzer/buzzer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"

static const char *TAG = "NewTimerManager";

static const uint32_t WATCHDOG_TIMEOUT_MS = 10 * 1000; 

static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alert_timer = NULL;
static TimerHandle_t update_watchdog_timer = NULL;

static uint16_t therapy_timer_duration = DEFAULT_THERAPY_DURATION;

static void (*timer_end_callback)(NotificationType) = NULL;
static void (*passed_duration_update_callback)() = NULL;

typedef enum {
    EVT_WATCHDOG_TICK,
    EVT_THERAPY_COMPLETED,
    EVT_ALERT_EXPIRED,
    EVT_INACTIVITY_EXPIRED
} TimerCompletedEvent;

QueueHandle_t timer_completed_event_queue = NULL;

static inline void post_timer_completed_event(TimerCompletedEvent ev) {
    if (!timer_completed_event_queue) return;

    if (xQueueSend(timer_completed_event_queue, &ev, pdMS_TO_TICKS(20)) != pdTRUE) {
        ESP_LOGE(TAG, "Timer completed event queue full! ev=%d", (int)ev);
    }
}

static void stop_duration_update_watchdog_timer() {
    if (update_watchdog_timer == NULL) {
        ESP_LOGD(TAG, "No watchdog to stop.");
        return;
    }
    if (xTimerIsTimerActive(update_watchdog_timer)) {
        xTimerStop(update_watchdog_timer, pdMS_TO_TICKS(50));
    }
}

static void update_watchdog_timeout_callback(TimerHandle_t xTimer) {
    post_timer_completed_event(EVT_WATCHDOG_TICK);
}

static void start_duration_update_watchdog_timer(void) {
    if (!update_watchdog_timer) {
        update_watchdog_timer = xTimerCreate("UpdateWatchdog",
            pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS), pdTRUE, NULL, update_watchdog_timeout_callback);
    }
    if (xTimerIsTimerActive(update_watchdog_timer)) {
        xTimerStop(update_watchdog_timer, 0);
    }
    xTimerStart(update_watchdog_timer, pdMS_TO_TICKS(50));
}

static void therapy_timer_expiry_callback(TimerHandle_t xTimer) {
    post_timer_completed_event(EVT_THERAPY_COMPLETED);
}

static void alert_timer_expiry_callback(TimerHandle_t xTimer) {
    post_timer_completed_event(EVT_ALERT_EXPIRED);
}

static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    post_timer_completed_event(EVT_INACTIVITY_EXPIRED);
}

bool stop_therapy_timer() {
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

bool start_therapy_timer(uint16_t duration) {
    ESP_LOGI(TAG, "Start therapy timer with: %u", duration);
    therapy_timer_duration = duration;
    if(therapy_timer != NULL) {
        stop_therapy_timer();
        ESP_LOGE(TAG, "Therapy is already started.");
    }

    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);

    start_duration_update_watchdog_timer();

    return therapy_timer != NULL;
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

bool start_inactivity_timer() {
    if(inactivity_timer != NULL) {
        stop_inactivity_timer();
        ESP_LOGE(TAG, "Inactivity timer is already started.");
    }

    inactivity_timer = create_and_start_timer(STATE_INACTIVE, INACTIVITY_THRESHOLD_SECONDS * 1000, inactivity_timer_expiry_callback);
    
    return inactivity_timer != NULL;
}

static bool stop_alert_timer()
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

bool start_alert_timer() {
    if(alert_timer != NULL) {
        stop_alert_timer();
        ESP_LOGE(TAG, "Alert timer is already started.");
    }
    alert_timer = create_and_start_timer(STATE_TEMPERATURE_ALERT, ALERT_THRESHOLD_SECONDS * 1000, alert_timer_expiry_callback);

    return alert_timer != NULL;
}

static uint32_t get_therapy_timer_remaining_ms(void) {
    if (!therapy_timer) return 0;

    if (xTimerIsTimerActive(therapy_timer) != pdTRUE) {
        return (uint32_t)therapy_timer_duration * 1000u;
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

    uint32_t nominal = (uint32_t)therapy_timer_duration * 1000u;

    if (ms + 1000u > nominal) {
        return nominal;
    }

    if (ms > nominal) return nominal;

    return ms;
}

uint32_t get_therapy_timer_passed_ms(void) {
    uint32_t total_ms = (uint32_t)therapy_timer_duration * 1000u;
    uint32_t remain_ms = get_therapy_timer_remaining_ms();
    if (remain_ms >= total_ms) return 0;
    return total_ms - remain_ms;
}

uint16_t get_therapy_timer_remaining_s(void) {
    uint32_t ms = get_therapy_timer_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

static uint32_t get_inactivity_timer_remaining_ms(void) {
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
        return (uint32_t)INACTIVITY_THRESHOLD_SECONDS * 1000u;

    #ifdef pdTICKS_TO_MS
    return (uint32_t)pdTICKS_TO_MS(remain);
    #else
    return (uint32_t)remain * (uint32_t)portTICK_PERIOD_MS;
    #endif
}

uint16_t get_inactivity_timer_remaining_s(void) {
    uint32_t ms = get_inactivity_timer_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

static uint32_t get_alert_timer_remaining_ms(void) {
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

uint16_t get_alert_timer_remaining_s(void) {
    uint32_t ms = get_alert_timer_remaining_ms();
    return (uint16_t)((ms + 999u) / 1000u);
}

void restart_duration_update_watchdog_timer(void) {
    if (update_watchdog_timer) {
        xTimerStop(update_watchdog_timer, 0);
        xTimerStart(update_watchdog_timer, 0);
    }
}

void register_passed_duration_update(void (*callback)()) {
    passed_duration_update_callback = callback;
}

void register_timer_end_callback(void (*callback)(NotificationType)) {
    timer_end_callback = callback;
}

static void timer_controller_task(void *arg) {
    TimerCompletedEvent ev;
    for (;;) {
        if (xQueueReceive(timer_completed_event_queue, &ev, portMAX_DELAY)) {
            switch (ev) {
            case EVT_WATCHDOG_TICK:
                if(passed_duration_update_callback) {
                    passed_duration_update_callback();
                }
                vTaskDelay(1);
                break;
            case EVT_THERAPY_COMPLETED:
                if (timer_end_callback) timer_end_callback(NOTIF_THERAPY_COMPLETED);
                vTaskDelay(1);
                break;
            case EVT_ALERT_EXPIRED:
                if (timer_end_callback) timer_end_callback(NOTIF_ALERT_TIMER_EXPIRED);
                vTaskDelay(1);
                break;
            case EVT_INACTIVITY_EXPIRED:
                if (timer_end_callback) timer_end_callback(NOTIF_INACTIVITY_TIMER_EXPIRED);
                vTaskDelay(1);
                break;
            }
        }
    }
}

void init_timer_controller() {
    timer_completed_event_queue = xQueueCreate(16, sizeof(TimerCompletedEvent));
    xTaskCreate(timer_controller_task, "timer_controller_task", 4096, NULL, 5, NULL);
}