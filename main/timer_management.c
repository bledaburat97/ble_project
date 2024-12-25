#include "timer_management.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "time.h"

#include "storage_management.h"

static const char *TAG = "TimerManagement";
static const char *LAST_THERAPY_END_TIME = "last_therapy_end_time";
static const char *LAST_THERAPY_APPLIED_DURATION = "last_therapy_applied_duration";

static time_t therapy_start_time = 0;
bool isDeviceRunning = false;
static TimerHandle_t therapy_timer = NULL;
static TaskHandle_t periodic_saving_task_handle = NULL;


static void save_current_time_and_applied_therapy_duration(){
    time_t current_time;
    time(&current_time); // Get the current time in seconds
    save_parameter(LAST_THERAPY_END_TIME, &current_time, sizeof(current_time));
    time_t elapsed_time = current_time - therapy_start_time;
    save_parameter(LAST_THERAPY_APPLIED_DURATION, &elapsed_time, sizeof(elapsed_time));
}

static void timer_expiry_callback(TimerHandle_t xTimer) {
    stop_therapy_timer();
}

static void periodic_saving_task(void *param) {
    while (isDeviceRunning) {
        save_current_time_and_applied_therapy_duration();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

static uint32_t get_last_therapy_end_time() {
    uint32_t last_therapy_end_time; 
    if (read_parameter(LAST_THERAPY_END_TIME, &last_therapy_end_time, sizeof(last_therapy_end_time)) == ESP_OK) {
        ESP_LOGI(TAG, "Last therapy end time: %lu", last_therapy_end_time);
    }
    else{
        ESP_LOGI(TAG, "Last therapy end time can not be found.");
    }
    return last_therapy_end_time;
}

static uint32_t get_last_therapy_applied_duration() {
    uint32_t last_therapy_applied_duration; 
    if (read_parameter(LAST_THERAPY_APPLIED_DURATION, &last_therapy_applied_duration, sizeof(last_therapy_applied_duration)) == ESP_OK) {
        ESP_LOGI(TAG, "Last therapy applied duration: %lu", last_therapy_applied_duration);
    }
    else{
        ESP_LOGI(TAG, "Last therapy applied duration can not be found.");
    }
    return last_therapy_applied_duration;
}

void start_therapy_timer(uint32_t duration) {
    if (therapy_timer == NULL) {
        // Create the timer if it does not exist
        therapy_timer = xTimerCreate(
            "TherapyTimer",           // Timer name
            pdMS_TO_TICKS(duration * 1000),  // Timer period in ticks
            pdFALSE,                     // One-shot timer
            NULL,                        // Timer ID (not used)
            timer_expiry_callback        // Callback function
        );

        if (therapy_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create therapy timer.");
            return;
        }
    }

    therapy_start_time = time(NULL);    
    isDeviceRunning = true;

    // Start periodic saving task
    if (periodic_saving_task_handle == NULL) {
        xTaskCreate(periodic_saving_task, "PeriodicSavingTask", 4096, NULL, 5, &periodic_saving_task_handle);
    }

    // Start or reset the timer
    if (xTimerStart(therapy_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Therapy timer started for %lu s.", duration);
    } else {
        ESP_LOGE(TAG, "Failed to start activation timer.");
    }
}

void stop_therapy_timer()
{
    isDeviceRunning = false;
    if (periodic_saving_task_handle != NULL) {
        vTaskDelete(periodic_saving_task_handle);
        periodic_saving_task_handle = NULL;
    }
}

void get_last_therapy_data(uint8_t *buffer) {
    if (buffer == NULL) return;

    uint32_t end_time = get_last_therapy_end_time();
    uint32_t applied_duration = get_last_therapy_applied_duration();
    uint16_t therapy_id = 0x0000; //TODO:get last_therapy_id from storage.

    buffer[0] = (end_time >> 24) & 0xFF;
    buffer[1] = (end_time >> 16) & 0xFF;
    buffer[2] = (end_time >> 8) & 0xFF;
    buffer[3] = end_time & 0xFF;

    buffer[4] = (applied_duration >> 24) & 0xFF;
    buffer[5] = (applied_duration >> 16) & 0xFF;
    buffer[6] = (applied_duration >> 8) & 0xFF;
    buffer[7] = applied_duration & 0xFF;

    buffer[8] = (therapy_id >> 8) & 0xFF;
    buffer[9] = therapy_id & 0xFF;
}

