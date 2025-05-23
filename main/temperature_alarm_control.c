#include "temperature_alarm_control.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "driver/gpio.h"
#include "state_manager.h"
#include "timer_management.h"

static const char *TAG = "TemperatureAlarm";

static bool normal_pin_status;
static uint8_t *alarm_gpio_list = NULL;
static uint8_t active_temp_sensor_count;

void set_alert_pin_normal_status(bool status) {
    normal_pin_status = status;
}

void set_alarm_gpios(const uint8_t* alarm_gpios, uint8_t count) {
    if (alarm_gpio_list) {
        free(alarm_gpio_list);
    }
    alarm_gpio_list = (uint8_t*)malloc(count * sizeof(uint8_t));
    if (alarm_gpio_list == NULL) {
        return;
    }
    memcpy(alarm_gpio_list, alarm_gpios, count * sizeof(uint8_t));
    active_temp_sensor_count = count;
}

void monitor_alert_task(void *param) {
    int prev_level[active_temp_sensor_count];
    for(int i = 0; i < active_temp_sensor_count; i++){
        prev_level[i] = gpio_get_level(alarm_gpio_list[i]);
        ESP_LOGI(TAG, "Current ALERT PIN %d: %s", i, prev_level[i] ? "HIGH (NORMAL)" : "LOW");
    }

    while (1) {
        int current_level[active_temp_sensor_count];
        for(int i = 0; i < active_temp_sensor_count; i++) {
            current_level[i] = gpio_get_level(alarm_gpio_list[i]);

            if (current_level[i] != prev_level[i]) {
                if(current_level[i] != normal_pin_status)
                {
                    if (get_device_state() != STATE_TEMPERATURE_ALARM){
                        start_alert_timer(i);
                    }
                    ESP_LOGI(TAG, "Temperature ALERT is triggered");
                }
                else
                {
                    ESP_LOGI(TAG, "Temperature is on normal level.");
                }
                prev_level[i] = current_level[i];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void initialize_alert_gpios() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT; 
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    for (int i = 0; i < active_temp_sensor_count; i++) {
        io_conf.pin_bit_mask = (1ULL << alarm_gpio_list[i]);
        gpio_config(&io_conf);
        ESP_LOGI(TAG, "Temperature sensor %d ALERT pin monitoring started on GPIO %d", i, alarm_gpio_list[i]);
    }
}

bool check_alert_status() {
    int sensor_level[active_temp_sensor_count];
    for(int i = 0; i < active_temp_sensor_count; i++){
        sensor_level[i] = gpio_get_level(alarm_gpio_list[i]);
        ESP_LOGI(TAG, "Current ALERT PIN %d: %s", i, sensor_level[i] ? "HIGH (NORMAL)" : "LOW");
        if(!sensor_level[i]){
            return true;
        }
    }
    return false;
}
