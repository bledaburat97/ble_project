#include "temp_alert_setter.h"

#include "temp_sensor_config.h"
#include "../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "TempAlertSetter";

static bool s_normal_pin_status;
static const gpio_num_t s_alert_gpio_list[] = { SECOND_ALERT_GPIO, FIRST_ALERT_GPIO, THIRD_ALERT_GPIO };

static void (*s_temp_alert_callback)(uint8_t)  = NULL;
static void (*s_temp_normal_callback)(uint8_t) = NULL;

static int s_prev_level[TEMPERATURE_SENSOR_COUNT];
static int s_current_level[TEMPERATURE_SENSOR_COUNT];

// ALERT pini "normal" seviyeden sapinca tetiklenen callback'ler.
void temp_alert_setter_register_temperature_alert(void (*callback)(uint8_t))
{
    s_temp_alert_callback = callback;
}

// ALERT pini normale donunce tetiklenen callback.
void temp_alert_setter_register_temperature_normal(void (*callback)(uint8_t))
{
    s_temp_normal_callback = callback;
}

// Sensorlerin ALERT pinlerini input olarak konfigüre eder.
static void initialize_alert_gpios(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        io_conf.pin_bit_mask = (1ULL << s_alert_gpio_list[i]);
        gpio_config(&io_conf);
        ESP_LOGI(TAG, "Temp sensor %d ALERT pin on GPIO %d", i, s_alert_gpio_list[i]);
    }
}

// ALERT pinlerinin normal seviyesini kaydeder ve GPIO'laro hazırlar.
void temp_alert_setter_initialize(bool normal_pin_status)
{
    s_normal_pin_status = normal_pin_status;
    initialize_alert_gpios();
}

// ALERT pinlerini periyodik izleyip değişimi callback'lere bildirir.
void temp_alert_setter_monitor_alert_task(void *param)
{
    (void)param;

    // Açılışta anormal seviyeleri yakalayip aninda bildir.
    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        s_prev_level[i] = gpio_get_level(s_alert_gpio_list[i]);
        if (s_prev_level[i] != (int)s_normal_pin_status) {
            ESP_LOGI(TAG, "Initial ALERT on sensor %d", i);
            if (s_temp_alert_callback) s_temp_alert_callback((uint8_t)i);
        }
    }

    while (1) {
        for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
            s_current_level[i] = gpio_get_level(s_alert_gpio_list[i]);
            if (s_current_level[i] != s_prev_level[i]) {
                if (s_current_level[i] != (int)s_normal_pin_status) {
                    ESP_LOGW(TAG, "Alerted sensor: %d", i);
                    if (s_temp_alert_callback) s_temp_alert_callback((uint8_t)i);
                } else {
                    ESP_LOGW(TAG, "Normal sensor: %d", i);
                    if (s_temp_normal_callback) s_temp_normal_callback((uint8_t)i);
                }
                s_prev_level[i] = s_current_level[i];
            }
        }
        // Donanım ALERT pinleri icin polling aralığı.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Herhangi bir sensor ALERT seviyesinde mi kontrol eder.
bool temp_alert_setter_check_alert_status(void)
{
    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        int level = gpio_get_level(s_alert_gpio_list[i]);
        if (level != (int)s_normal_pin_status) return true;
    }
    return false;
}
