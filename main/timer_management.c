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
static const char *LAST_THERAPY_APPLIED_DURATION = "last_ther_dur";

static time_t therapy_start_time = 0;
static TimerHandle_t therapy_timer = NULL;
static TimerHandle_t inactivity_timer = NULL;
static TaskHandle_t periodic_saving_task_handle = NULL;
static void (*timer_notification_callback)(NotificationType) = NULL;

static bool is_therapy_active = false; // remove it  

static void save_current_time_and_applied_therapy_duration(){
    time_t current_time;
    time(&current_time);

    uint16_t elapsed_time_32 = (uint16_t)(current_time - therapy_start_time);
    save_parameter(LAST_THERAPY_APPLIED_DURATION, &elapsed_time_32, sizeof(elapsed_time_32));
}

static void therapy_timer_expiry_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Therapy timer expired!");
    stop_therapy_timer();
    if (timer_notification_callback) {
        timer_notification_callback(TIMER_ENDED);
    }
}

static void inactivity_timer_expiry_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Inactivity timer expired!");

    //TODO: turn off the device. 
}

static void periodic_saving_task(void *param) {
    while (is_therapy_active) {
        save_current_time_and_applied_therapy_duration();
        vTaskDelay(pdMS_TO_TICKS(1000));
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
    return last_therapy_applied_duration;
}

void register_timer_notification_callback(void (*callback)(NotificationType)) {
    timer_notification_callback = callback;
}

void start_inactivity_timer() {
    if(is_therapy_active) {
        return;    
    }

    if(inactivity_timer == NULL) {
        inactivity_timer = xTimerCreate(
            "InactivityTimer",
            pdMS_TO_TICKS(INACTIVITY_THRESHOLD_SECONDS * 1000),  
            pdFALSE,
            NULL,                        
            inactivity_timer_expiry_callback        
        );

        if (inactivity_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create inactivity timer.");
            return;
        }
    }

    if (xTimerStart(inactivity_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Inactivity timer started for %u s.", INACTIVITY_THRESHOLD_SECONDS);
    } else {
        ESP_LOGE(TAG, "Failed to start inactivity timer.");
    }
}

static void stop_inactivity_timer() {
    if (inactivity_timer == NULL) {
        ESP_LOGW(TAG, "Inactivity timer has not been created.");
        return;
    }

    if (xTimerStop(inactivity_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Inactivity timer stopped.");
    } else {
        ESP_LOGE(TAG, "Failed to stop inactivity timer.");
    }
    inactivity_timer = NULL;
}

void restart_inactivity_timer() {
    stop_inactivity_timer();
    start_inactivity_timer();
}

static uint16_t convert_bit_string_to_duration_in_seconds(uint16_t duration_bits) {
    if(duration_bits > 480) {
        ESP_LOGE(TAG, "Wrong therapy duration is got.");
        duration_bits = 480;
    }
    return duration_bits * 5;
}

void start_therapy_timer(uint16_t duration_bits) {
    stop_inactivity_timer();
    uint16_t duration = convert_bit_string_to_duration_in_seconds(duration_bits);
    if (therapy_timer == NULL) {
        // Create the timer if it does not exist
        therapy_timer = xTimerCreate(
            "TherapyTimer",           // Timer name
            pdMS_TO_TICKS(duration * 1000),  // Timer period in ticks
            pdFALSE,                     // One-shot timer
            NULL,                        // Timer ID (not used)
            therapy_timer_expiry_callback        // Callback function
        );

        if (therapy_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create therapy timer.");
            return;
        }
    }

    therapy_start_time = time(NULL);    
    is_therapy_active = true;

    // Start periodic saving task
    if (periodic_saving_task_handle == NULL) {
        xTaskCreate(periodic_saving_task, "PeriodicSavingTask", 4096, NULL, 5, &periodic_saving_task_handle);
    }

    // Start or reset the timer
    if (xTimerStart(therapy_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Therapy timer started for %u s.", duration);
    } else {
        ESP_LOGE(TAG, "Failed to start activation timer.");
    }
}

void stop_therapy_timer()
{
    if (therapy_timer == NULL) {
        ESP_LOGW(TAG, "Therapy timer has not been created.");
        return;
    }

    is_therapy_active = false;

    if (periodic_saving_task_handle != NULL) {
        vTaskDelete(periodic_saving_task_handle);
        periodic_saving_task_handle = NULL;
    }

    if (xTimerStop(therapy_timer, 0) == pdPASS) {
        ESP_LOGI(TAG, "Therapy timer stopped.");
    } else {
        ESP_LOGE(TAG, "Failed to stop therapy timer.");
    }
    therapy_timer = NULL;
    start_inactivity_timer();
}



