#include "deep_sleep_manager.h"

#include "state_manager.h"

#include "../i2c/laser/laser_driver_controller.h"

#include "../device_configuration.h"

#include "driver/rtc_io.h" 
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DeepSleepManager";

void set_deep_sleep_button() {
    gpio_config_t io_conf = {
        .pin_bit_mask = BUTTON_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

void enter_deep_sleep() {
    ESP_LOGI(TAG, "Entering to deep sleep");
    set_laser_drivers_status(false);
    set_deep_sleep_button();
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}


