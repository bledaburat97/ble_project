#include "main_button_controller.h"

#include "../state/deep_sleep_manager.h"
#include "../state/state_manager.h"
#include "../state/timer_manager.h"
#include "../state/current_therapy_info_manager.h"
#include "../state/mode_selector.h"

#include "../transaction/incoming_message_handler.h"
#include "../transaction/notification_info_message_creator.h"
#include "../transaction/default_configuration_handler.h"

#include "../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


static const TickType_t SHORT_PRESS_WINDOW = pdMS_TO_TICKS(1000);
static uint16_t s_short_press_count = 0;
static TickType_t s_last_short_press_tick = 0;

static const char *TAG = "MainButtonController";

static void start_or_pause_therapy() {
    if (get_device_state() == STATE_INACTIVE) {
        start_or_continue_therapy(false);
    } else if (get_device_state() == STATE_ACTIVE) {
        pause_therapy();
        add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_BUTTON);
    } else {
        ESP_LOGW(TAG, "SHORT: Beklenmeyen state!");
    }
}

static void on_short_press_sequence(uint16_t count) {
    if(count == 1) {
        start_or_pause_therapy();
        return;
    }
    change_default_parameters(count);
}

static inline void track_short_press(void) {
    TickType_t now = xTaskGetTickCount();

    if (s_short_press_count == 0) {
        s_short_press_count = 1;
    } else {
        if ((now - s_last_short_press_tick) <= SHORT_PRESS_WINDOW) {
            s_short_press_count++;
        } else {
            on_short_press_sequence(s_short_press_count);
            ESP_LOGI(TAG, "Short press sequence finished (timeout): count=%lu", (unsigned long)s_short_press_count);
            s_short_press_count = 1;
        }
    }
    s_last_short_press_tick = now;
}

static inline void maybe_finalize_short_press_sequence(void) {
    if (s_short_press_count > 0) {
        TickType_t now = xTaskGetTickCount();
        if ((now - s_last_short_press_tick) > SHORT_PRESS_WINDOW) {
            on_short_press_sequence(s_short_press_count);
            ESP_LOGI(TAG, "Short press sequence finished (idle): count=%lu", (unsigned long)s_short_press_count);
            s_short_press_count = 0;
        }
    }
}

void do_short_press(void) {
    if(get_indicator_led_status()) {
        track_short_press();
        return;
    }
    start_or_pause_therapy();
}

void wait_for_button_to_sleep(void *pvParameters) {
    const TickType_t poll_delay_ticks = pdMS_TO_TICKS(100);
    const TickType_t long_press_ticks = pdMS_TO_TICKS(PRESS_DURATION_TO_CONFIGURATION_MS);
    const TickType_t very_long_press_ticks = pdMS_TO_TICKS(PRESS_DURATION_TO_SLEEP_MS);

    TickType_t press_start = 0;
    bool button_was_pressed = false;

    while (1) {
        int level = gpio_get_level(BUTTON_GPIO);

        if (level == 0) {
            if (!button_was_pressed) {
                press_start = xTaskGetTickCount();
                button_was_pressed = true;
                ESP_LOGI(TAG, "Button is pressed.");
            }
        } else if (button_was_pressed) {
            TickType_t press_duration_ticks = xTaskGetTickCount() - press_start;
            button_was_pressed = false;

#ifdef pdTICKS_TO_MS
            uint32_t duration_ms = (uint32_t)pdTICKS_TO_MS(press_duration_ticks);
#else
            uint32_t duration_ms = (uint32_t)press_duration_ticks * (uint32_t)portTICK_PERIOD_MS;
#endif
        ESP_LOGI(TAG, "Button is released, the passed duration: %lu ms", (unsigned long)duration_ms);

            if(press_duration_ticks >= very_long_press_ticks) {
                ESP_LOGI(TAG, "LONG: Enter deep sleep");
                enter_deep_sleep();
            } else if (press_duration_ticks >= long_press_ticks) {
                change_mode_indicator_gpio_pin_status();
            } else {
                do_short_press();
            }
        }

        maybe_finalize_short_press_sequence();

        vTaskDelay(poll_delay_ticks);
    }
}