#include "helmet_off_debounce_timer_manager.h"

#define HELMET_OFF_DEBOUNCE_MS 1000

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static TimerHandle_t helmet_off_debounce_timer = NULL;
static const char *TAG = "HelmetOffDebounceTimerManager";

bool stop_helmet_off_debounce_timer(void) {
    if (helmet_off_debounce_timer == NULL) {
        return true;
    }

    if (xTimerIsTimerActive(helmet_off_debounce_timer) == pdTRUE) {
        if (xTimerStop(helmet_off_debounce_timer, 0) != pdPASS) {
            ESP_LOGE(TAG, "Failed to stop helmet off debounce timer");
            return false;
        }
    }
    return true;
}

bool is_helmet_off_debounce_timer_running(void) {
    return (helmet_off_debounce_timer != NULL) && (xTimerIsTimerActive(helmet_off_debounce_timer) == pdTRUE);
}

static void helmet_off_debounce_timer_cb(TimerHandle_t t)
{
    (void)t; // bilerek boş bırakıldı
}

bool start_helmet_off_debounce_timer(void) {
    if (helmet_off_debounce_timer == NULL) {
        helmet_off_debounce_timer = xTimerCreate(
            "HelmetOffDebounce",
            pdMS_TO_TICKS(HELMET_OFF_DEBOUNCE_MS),
            pdFALSE,
            NULL,
            helmet_off_debounce_timer_cb
        );

        if (helmet_off_debounce_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create helmet off debounce timer");
            return false;
        }
    }

    // Zaten çalışıyorsa önce durdur, sonra yeniden başlat
    if (xTimerIsTimerActive(helmet_off_debounce_timer) == pdTRUE) {
        if (xTimerStop(helmet_off_debounce_timer, 0) != pdPASS) {
            ESP_LOGW(TAG, "Failed to stop helmet off debounce timer before restart");
        }
    }

    if (xTimerStart(helmet_off_debounce_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start helmet off debounce timer");
        return false;
    }

    return true;
}