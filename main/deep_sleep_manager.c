#include "driver/rtc_io.h" 
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "deep_sleep_manager.h"
#include "state_manager.h"

static const char *TAG = "DeepSleepManager";

void enter_deep_sleep() {
    ESP_LOGI(TAG, "Deep sleep'e geçiliyor...");
    
    // GPIO0 pull-up + input mod
    gpio_config_t io_conf = {
        .pin_bit_mask = BUTTON_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);

    esp_deep_sleep_start();
}


