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

static const char *TAG = "TimerManagement";
static const char *LAST_THERAPY_APPLIED_DURATION = "last_ther_dur";

static time_t therapy_start_time = 0;
static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TimerHandle_t alarm_timer = NULL;
static uint16_t passed_duration = 0;

static TaskHandle_t periodic_saving_task_handle = NULL;
static void (*timer_notification_callback)(NotificationType) = NULL;

static void save_applied_therapy_duration(){

    ESP_LOGI(TAG, "save applied therapy duration!");

    time_t current_time;
    time(&current_time);

    passed_duration = (uint16_t)(current_time - therapy_start_time);
    save_parameter(LAST_THERAPY_APPLIED_DURATION, &passed_duration, sizeof(passed_duration));
}

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
    if (timer_notification_callback) {
        timer_notification_callback(THERAPY_COMPLETED);
    }
    start_inactivity_timer();
}

static void stop_inactivity_timer() {
    if(stop_and_delete_timer(&inactivity_timer)){
        ESP_LOGI(TAG, "Inactivity timer is stopped.");
        set_device_state(STATE_UNKNOWN);
    }
    else{
        ESP_LOGI(TAG, "Inactivity timer can not be stopped.");
    }
}
static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Inactivity timer expired!");
    stop_inactivity_timer();
    if (timer_notification_callback) {
        timer_notification_callback(INACIVITY_TIMER_COMPLETED);
    }
    enter_deep_sleep();
}

static void alarm_timer_expiry_callback(TimerHandle_t xTimer) {
    set_device_state(STATE_START);
}

static void periodic_saving_task(void *param) {
    while (is_therapy_timer_running()) {
        ESP_LOGI(TAG, "periodic_saving_task!");
        save_applied_therapy_duration();
        vTaskDelay(pdMS_TO_TICKS(PERIODIC_SAVING_INTERVAL));
    }
    vTaskDelete(NULL);
}

uint16_t get_last_therapy_applied_duration() {
    uint16_t last_therapy_applied_duration; 
    if (read_parameter(LAST_THERAPY_APPLIED_DURATION, &last_therapy_applied_duration, sizeof(last_therapy_applied_duration)) == ESP_OK) {
        ESP_LOGI(TAG, "Last therapy applied duration: %u", last_therapy_applied_duration);
    }
    else{
        ESP_LOGI(TAG, "Last therapy applied duration can not be found.");
    }
    if(last_therapy_applied_duration > MAX_THERAPY_APPLIED_DURATION){
        last_therapy_applied_duration = MAX_THERAPY_APPLIED_DURATION;
    }
    return last_therapy_applied_duration;
}

void register_timer_notification_callback(void (*callback)(NotificationType)) {
    timer_notification_callback = callback;
}

static uint16_t convert_bit_string_to_duration_in_seconds(uint16_t duration_bits) {
    if(duration_bits > 480) {
        ESP_LOGE(TAG, "Wrong therapy duration is got.");
        duration_bits = 480;
    }
    return duration_bits * 5;
}

void start_alarm_timer() {
    if (!alarm_timer) {
        alarm_timer = create_and_start_timer(STATE_TEMPERATURE_ALARM, ALARM_THRESHOLD_SECONDS * 1000, alarm_timer_expiry_callback);
    }
    set_device_state(STATE_TEMPERATURE_ALARM);
}

static void stop_therapy_timer()
{
    if(stop_and_delete_timer(&therapy_timer)){
        ESP_LOGI(TAG, "Therapy timer is stopped.");
        set_device_state(STATE_UNKNOWN);
    }
    else{
        ESP_LOGI(TAG, "Therapy timer can not be stopped.");
    }

    if (periodic_saving_task_handle != NULL) {
        vTaskDelete(periodic_saving_task_handle);
        periodic_saving_task_handle = NULL;
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
}
    


static void start_therapy_timer(uint16_t duration) {
    
    therapy_timer = create_and_start_timer(STATE_ACTIVE, duration * 1000, therapy_timer_expiry_callback);
    
    set_device_state(STATE_ACTIVE);

    therapy_start_time = time(NULL);    

    // Start periodic saving task
    if (periodic_saving_task_handle == NULL) {
        xTaskCreate(periodic_saving_task, "PeriodicSavingTask", 4096, NULL, 5, &periodic_saving_task_handle);
    }
}

void set_and_start_therapy_timer(uint16_t duration_bits) {
    if(is_alarm_timer_running()) {
        ESP_LOGE(TAG, "Alarm timer wasn't stopped.");
        return;
    }

    if(is_inactivity_timer_running()) {
        stop_inactivity_timer();
    }

    if(is_therapy_timer_running()) {
        stop_therapy_timer();
    }

    ESP_LOGI(TAG, "Start therapy timer with duration bits: %u", duration_bits);

    uint16_t duration = convert_bit_string_to_duration_in_seconds(duration_bits);
    start_therapy_timer(duration);
}

void set_and_start_default_therapy_timer() {
    if(is_alarm_timer_running()) {
        ESP_LOGE(TAG, "Alarm timer wasn't stopped.");
        return;
    }

    if(is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Therapy timer wasn't stopped.");
        return;
    }

    if(is_inactivity_timer_running()) {
        stop_inactivity_timer();
    }

    uint16_t duration = DEFAULT_THERAPY_DURATION - passed_duration;
    start_therapy_timer(duration);
}



static void on_helmet_state_changed(bool helmet_state) {
    if (!helmet_state) {
        if (get_device_state == STATE_ACTIVE) {
            start_inactivity_timer();
        }
    }
}

void init_timer_manager()
{
    passed_duration = 0;
    register_helmet_state_change_callback(on_helmet_state_changed);
}