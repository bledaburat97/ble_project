#include "constants.h"
#include "temperature_alarm_control.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TemperatureAlarm";

static bool normal_pin_status;

void set_alert_pin_normal_status(bool status) {
    normal_pin_status = status;
}

void monitor_alert_task(void *param) {
    int prev_level = gpio_get_level(TEMP_SENSOR_ALERT_GPIO);
    ESP_LOGI(TAG, "Current ALERT PIN: %s", prev_level ? "HIGH (NORMAL)" : "LOW");

    while (1) {
        int current_level = gpio_get_level(TEMP_SENSOR_ALERT_GPIO);

        if (current_level != prev_level) {
            if(current_level != normal_pin_status)
            {
                ESP_LOGI(TAG, "Temperature ALERT is triggered");
            }
            else
            {
                ESP_LOGI(TAG, "Temperature is on normal level.");
            }
            prev_level = current_level;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initialize_alert_gpio() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << TEMP_SENSOR_ALERT_GPIO);
    io_conf.mode = GPIO_MODE_INPUT; 
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Temperature sensor ALERT pin monitoring started on GPIO %d", TEMP_SENSOR_ALERT_GPIO);
}
