#include "temperature_alert_controller.h"

#include "../../state/state_manager.h"

#include "../../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "driver/gpio.h"

static const char *TAG = "TemperatureAlertController";

static bool normal_pin_status;
static const gpio_num_t alert_gpio_list[] = {SECOND_ALERT_GPIO, FIRST_ALERT_GPIO, THIRD_ALERT_GPIO};

static uint8_t active_temp_sensor_count = 0;
static void (*temp_alert_callback)(uint8_t) = NULL;
static void (*temp_normal_callback)(uint8_t) = NULL;

void register_temperature_alert(void (*callback)(uint8_t)) {
    temp_alert_callback = callback;
}

void register_temperature_normal(void (*callback)(uint8_t)) {
    temp_normal_callback = callback;
}

void set_alert_pin_normal_status(bool status) {
    normal_pin_status = status;
}

void set_active_temp_sensor_count(uint8_t count) {
    if (count > MAX_TEMP_SENSORS) {
        ESP_LOGW(TAG, "Requested count(%u) > MAX_TEMP_SENSORS(%u). Truncating.", count, MAX_TEMP_SENSORS);
        count = MAX_TEMP_SENSORS;
    }
    uint8_t max_list = sizeof(alert_gpio_list) / sizeof(alert_gpio_list[0]);
    if (count > max_list) count = max_list;

    active_temp_sensor_count = count;
}

static int prev_level[MAX_TEMP_SENSORS];
static int current_level[MAX_TEMP_SENSORS];

void monitor_alert_task(void *param) {

    if (active_temp_sensor_count == 0) {
        ESP_LOGE(TAG, "No sensors configured! Call set_alarm_gpios() before starting this task.");
        vTaskDelete(NULL);
        return;
    }

    for (int i = 0; i < active_temp_sensor_count; i++) {
        prev_level[i] = gpio_get_level(alert_gpio_list[i]);
        // Eğer başlangıç alert seviyesinde ise hemen tetikle
        if (prev_level[i] != (int)normal_pin_status) {
            ESP_LOGI(TAG, "Initial ALERT on sensor %d (pin %d)", i, alert_gpio_list[i]);
            if (temp_alert_callback) temp_alert_callback(i);
        }
    }

    while (1) {
        for(int i = 0; i < active_temp_sensor_count; i++) {
            current_level[i] = gpio_get_level(alert_gpio_list[i]);

            if (current_level[i] != prev_level[i]) {
                ESP_LOGI(TAG, "Current LEVEL PIN %d: %s", i, current_level[i] ? "HIGH (NORMAL)" : "LOW");
                if(current_level[i] != (int)normal_pin_status)
                {
                    ESP_LOGI(TAG, "Temperature ALERT is triggered");
                    if(temp_alert_callback) {
                        ESP_LOGI(TAG, "Alert callback is sent.");
                        temp_alert_callback(i);
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "Temperature is on normal level.");
                    if(temp_normal_callback) {
                        //ESP_LOGI(TAG, "Normal temp callback is sent.");
                        temp_normal_callback(i);
                    }
                }
                prev_level[i] = current_level[i];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void initialize_alert_gpios() {
    if (active_temp_sensor_count == 0) {
        ESP_LOGE(TAG, "No sensors configured! Call set_active_temp_sensor_count() first.");
        return;
    }
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT; 
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    for (int i = 0; i < active_temp_sensor_count; i++) {
        io_conf.pin_bit_mask = (1ULL << alert_gpio_list[i]);
        gpio_config(&io_conf);
        ESP_LOGI(TAG, "Temperature sensor %d ALERT pin monitoring started on GPIO %d", i, alert_gpio_list[i]);
    }
}

bool check_alert_status() {
    if (active_temp_sensor_count == 0) return false;
    
    for (int i = 0; i < active_temp_sensor_count; i++) {
        int level = gpio_get_level(alert_gpio_list[i]);
        ESP_LOGI(TAG, "Current alert pin of %d (%d): %s", i, alert_gpio_list[i], level ? "HIGH (NORMAL)" : "LOW");
        if (level != (int)normal_pin_status) {
            return true;
        }
    }
    return false;
}
