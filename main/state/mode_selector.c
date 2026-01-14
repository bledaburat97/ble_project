#include "mode_selector.h"

#include "../transaction/default_configuration_handler.h"
#include "../i2c/laser/laser_driver_controller.h"

#include "../manager/session_timer_getter.h"
#include "../manager/message_saver.h"

#include "../storage/log_types.h"

#include "esp_log.h"
#include "string.h"
#include "driver/gpio.h"

static const char *TAG = "ModeSelector";

static bool indicator_led_status = false;

void initialize_mode_indicator_gpio(){
    gpio_config_t io_conf_led_driver = {
        .pin_bit_mask = (1ULL << MODE_INDICATOR_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf_led_driver);
}

void change_mode_indicator_gpio_pin_status() {
    indicator_led_status = !indicator_led_status;
    gpio_set_level(MODE_INDICATOR_LED_GPIO, indicator_led_status);

    if(!indicator_led_status) {
        ESP_LOGI(TAG, "Indicator led is turning off.");
        const uint8_t *brightness_list = get_default_brightness();
        uint16_t passed_seconds = get_session_passed_seconds();
        change_brightness(brightness_list);
        save_log(NOTIF_BRIGHTNESS_UPDATED, brightness_list, 6, passed_seconds);
    }
    else{
        ESP_LOGI(TAG, "Indicator led is turning on.");
    }
}

void change_default_parameters(uint16_t press_count) {
    switch(press_count) {
        case 2:
        case 3:
        case 4:
        case 5:
            set_and_store_default_therapy_duration(300 * press_count);
            break;
        case 6:
            set_and_store_default_therapy_duration(INFINITE_THERAPY_DURATION);
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            uint8_t brightnessInPercent = (press_count - 6) * 25;
            uint8_t brightness[6] = {brightnessInPercent, brightnessInPercent, brightnessInPercent, brightnessInPercent, brightnessInPercent, brightnessInPercent};
            set_and_store_default_brightness(brightness);
            break;
        default:
            break;
    }
}

bool get_indicator_led_status() {
    return indicator_led_status;
}