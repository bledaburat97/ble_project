#include "driver/gpio.h"
#include "main_button_controller.h"
#include "deep_sleep_manager.h"
#include "state_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_management.h"
#include "transaction_manager.h"
static const char *TAG = "MainButtonController";

void wait_for_button_to_sleep(void *pvParameters) {
    TickType_t press_start = 0;
    TickType_t press_duration = 0;
    bool button_was_pressed = false;

    while (1) {
        int level = gpio_get_level(BUTTON_GPIO);

        if (level == 0) {
            if (!button_was_pressed) {
                press_start = xTaskGetTickCount();
                button_was_pressed = true;
                ESP_LOGI(TAG, "Button is pressed.");
            }
        } else {
            if (button_was_pressed) {
                press_duration = xTaskGetTickCount() - press_start;
                button_was_pressed = false;

                uint32_t duration_ms = press_duration * portTICK_PERIOD_MS;

                ESP_LOGI(TAG, "Button is released, the passed duration: %lu ms", duration_ms);

                if (duration_ms >= PRESS_DURATION_TO_SLEEP_MS) {
                    ESP_LOGI(TAG, "Entering deep sleep.");
                    enter_deep_sleep();
                } else {
                    if(get_device_state() == STATE_INACTIVITY) {
                        start_default_therapy();
                    }
                    else if(get_device_state() == STATE_ACTIVE) {
                        start_inactivity_timer();
                        add_and_send_notification(THERAPY_PAUSED_BY_BUTTON);
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // debounce
    }
}