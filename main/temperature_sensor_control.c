#include "i2c_control.h"
#include "temperature_sensor_control.h"
#include "esp_log.h"
#include "temperature_sensor.h"
#include "esp_task_wdt.h"
#include "temperature_alarm_control.h"
#include <math.h>

#define FIRST_PJ85775_ADDRESS 0x48
#define SECOND_PJ85775_ADDRESS 0x4C
#define THIRD_PJ85775_ADDRESS 0x4A

#define FIRST_ALERT_GPIO GPIO_NUM_3
#define SECOND_ALERT_GPIO GPIO_NUM_2
#define THIRD_ALERT_GPIO GPIO_NUM_5

#define NVS_KEY_TEMPERATURE "last_temp"
#define TEMPERATURE_SENSOR_COUNT 1 //TODO: Değiştir.
#define LOW_THRESHOLD_IN_NORMAL 25.5
#define LOW_THRESHOLD_IN_ALERT 27
#define HIGH_THRESHOLD_IN_NORMAL 27.5
#define HIGH_THRESHOLD_IN_ALERT 30

#define TEMPERATURE_DIFF_OFFSET 1

static const char *TAG = "TemperatureControl";
static const uint8_t sensor_addresses[] = {SECOND_PJ85775_ADDRESS, FIRST_PJ85775_ADDRESS, THIRD_PJ85775_ADDRESS};
static const uint8_t alarm_gpios[] = {SECOND_ALERT_GPIO, FIRST_ALERT_GPIO, THIRD_ALERT_GPIO};
static float current_temperature;
static void (*temp_update_callback)(uint8_t) = NULL;

static uint8_t convert_float_to_byte(float temperature) {
    //6 bit tam sayı (0 - 63)
    //son iki bit (0, 0.25, 0.5, 0.75) ifade eder.
    if (temperature < 0.0f || temperature > 64) {
        return 0xFF;
    }

    uint8_t integer_part = (uint8_t)temperature;
    float fractional_part = temperature - integer_part;

    uint8_t frac_bits;
    if (fractional_part < 0.25f) {
        frac_bits = 0b00;
    } else if (fractional_part < 0.5f) {
        frac_bits = 0b01;
    } else if (fractional_part < 0.75f) {
        frac_bits = 0b10;
    } else {
        frac_bits = 0b11;
    }

    uint8_t temperature_in_byte = (integer_part << 2) | frac_bits;
    return temperature_in_byte;
}

uint8_t log_temperature() {
    ESP_LOGI(TAG, "Log temperature.");
    float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[0]);
    return (uint8_t)temperatureInDegree;
}

void register_temperature_update(void (*callback)(uint8_t)) {
    temp_update_callback = callback;
}

static float measure_average_temperature() {
    float sum_of_temperatures = 0;
    for(int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[i]);
        sum_of_temperatures += temperatureInDegree;
    }
       
    return sum_of_temperatures / TEMPERATURE_SENSOR_COUNT;
}

uint8_t get_temperature() {
    float average_temperature = measure_average_temperature();
    current_temperature = average_temperature;
    return convert_float_to_byte(average_temperature);
}

static float round_down_to_half(float temp) {
    return floorf(temp * 2.0f) / 2.0f;
}

void temperature_read_task(void *param) {
    while (1) {
        float average_temperature = measure_average_temperature();
        float rounded_temperature = round_down_to_half(average_temperature);

        if (fabsf(current_temperature - rounded_temperature) >= 0.5f) {
            ESP_LOGI(TAG,"Temperature changed.");
            /*
            if(temp_update_callback) {
                temp_update_callback(convert_float_to_byte(average_temperature));
            }
            else {
                ESP_LOGE(TAG, "Temperature update can not be sent.");
            }
            */
        } 

        current_temperature = rounded_temperature;

        ESP_LOGI(TAG, "Measured temperature: %.2f°C", average_temperature);
        //ESP_LOGI(TAG, "Saved temperature: %.2f°C", rounded_temperature);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void increase_thresholds(uint8_t sensor_index) {
    ESP_LOGI(TAG, "Increase thresholds");
    set_threshold_temperature(sensor_addresses[sensor_index], LOW_THRESHOLD_IN_ALERT, LOW);
    set_threshold_temperature(sensor_addresses[sensor_index], HIGH_THRESHOLD_IN_ALERT, HIGH);
}


static void set_normal_thresholds(uint8_t sensor_index) {
    ESP_LOGI(TAG, "Set normal thresholds");
    set_threshold_temperature(sensor_addresses[sensor_index], LOW_THRESHOLD_IN_NORMAL, LOW);
    set_threshold_temperature(sensor_addresses[sensor_index], HIGH_THRESHOLD_IN_NORMAL, HIGH);
}

static void on_temp_alert_callback(uint8_t sensor_index) {
    float average_temperature = measure_average_temperature();
    float rounded_temperature = round_down_to_half(average_temperature);
    ESP_LOGI(TAG, "sensor index: %u", sensor_index);
    ESP_LOGI(TAG, "temperature: %.2f°C", average_temperature);
    ESP_LOGI(TAG, "Saved temperature: %.2f°C", rounded_temperature);
    increase_thresholds(sensor_index);
}

static void on_temp_normal_callback(uint8_t sensor_index) {
    float average_temperature = measure_average_temperature();
    float rounded_temperature = round_down_to_half(average_temperature);
    ESP_LOGI(TAG, "sensor index: %u", sensor_index);
    ESP_LOGI(TAG, "temperature: %.2f°C", average_temperature);
    ESP_LOGI(TAG, "Saved temperature: %.2f°C", rounded_temperature);
    set_normal_thresholds(sensor_index);
}


void initialize_temperature_sensor() {
    ESP_LOGI(TAG, "Initialize temperature sensors.");
    TempSensorConfigReg config;
    config.os = OS_DISABLED;
    config.cr = CR_4HZ;
    config.fq = FAULT_QUEUE_2;
    config.pol = ALERT_ACTIVE_HIGH;
    config.altm = ALERT_COMPARATOR_MODE;
    config.sd = NORMAL_MODE;
    set_alert_pin_normal_status(config.pol != ALERT_ACTIVE_HIGH);
    set_alarm_gpios(alarm_gpios, TEMPERATURE_SENSOR_COUNT);
    register_temperature_alert(on_temp_alert_callback);
    register_temperature_normal(on_temp_normal_callback);
    set_i2c_master_num(I2C_FIRST_MASTER_NUM);

    for(int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        set_configuration(sensor_addresses[i], config);
        set_normal_thresholds(i);
    }
}