#include "main_button_controller.h"

#include "../state/deep_sleep_manager.h"
#include "../state/state_manager.h"
#include "../state/timer_manager.h"
#include "../state/current_therapy_info_manager.h"

#include "../transaction/incoming_message_handler.h"
#include "../transaction/notification_info_message_creator.h"

#include "../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


static const char *TAG = "MainButtonController";

void do_short_press(void) {
    if (get_device_state() == STATE_INACTIVE) {
        start_or_continue_therapy(false);
    } else if (get_device_state() == STATE_ACTIVE) {
        pause_therapy();
        add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_BUTTON);
    } else {
        ESP_LOGW(TAG, "SHORT: Beklenmeyen state!");
    }
}

void do_long_press(void) {
    ESP_LOGI(TAG, "LONG: Enter deep sleep");
    enter_deep_sleep();
}


void wait_for_button_to_sleep(void *pvParameters) {
    const TickType_t poll_delay_ticks = pdMS_TO_TICKS(100);
    const TickType_t long_press_ticks = pdMS_TO_TICKS(PRESS_DURATION_TO_SLEEP_MS);
    
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

            if (press_duration_ticks >= long_press_ticks) {
                do_long_press();
            } else {
                do_short_press();
            }
        }

        vTaskDelay(poll_delay_ticks);
    }
}