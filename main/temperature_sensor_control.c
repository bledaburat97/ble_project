#include "i2c_control.h"
#include "temperature_sensor_control.h"
#include "esp_log.h"
#include "storage_management.h"
#include "temperature_sensor.h"
#include "esp_task_wdt.h"
#include "temperature_alarm_control.h"

#define FIRST_PJ85775_ADDRESS 0x48
#define SECOND_PJ85775_ADDRESS 0x49
#define THIRD_PJ85775_ADDRESS 0x4A

#define FIRST_ALERT_GPIO GPIO_NUM_20
#define SECOND_ALERT_GPIO GPIO_NUM_21
#define THIRD_ALERT_GPIO GPIO_NUM_22

#define NVS_KEY_TEMPERATURE "last_temp"
#define TEMPERATURE_SENSOR_COUNT 1 //TODO: Değiştir.
#define LOW_THRESHOLD 25.5
#define HIGH_THRESHOLD 27

static const char *TAG = "TemperatureControl";
static const uint8_t sensor_addresses[] = {FIRST_PJ85775_ADDRESS, SECOND_PJ85775_ADDRESS, THIRD_PJ85775_ADDRESS};
static const uint8_t alarm_gpios[] = {FIRST_ALERT_GPIO, SECOND_ALERT_GPIO, THIRD_ALERT_GPIO};

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
    set_i2c_master_num(I2C_FIRST_MASTER_NUM);

    for(int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        set_configuration(sensor_addresses[i], config);
        set_threshold_temperature(sensor_addresses[i], LOW_THRESHOLD, LOW);
        set_threshold_temperature(sensor_addresses[i], HIGH_THRESHOLD, HIGH);
    }

    float last_stored_temp;
    if (read_parameter(NVS_KEY_TEMPERATURE, &last_stored_temp, sizeof(float)) == ESP_OK) {
        ESP_LOGI(TAG, "Last saved temperature: %.2f°C", last_stored_temp);
    } else {
        ESP_LOGE(TAG, "Last saved temperature can not be got.");
    }
}

uint8_t log_temperature() {
    ESP_LOGI(TAG, "Log temperature.");
    float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[0]);
    return (uint8_t)temperatureInDegree;
}

void temperature_update_task(void *param) {
    while (1) {
        float sum_of_temperatures = 0;
        for(int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
            float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[i]);
            sum_of_temperatures += temperatureInDegree;
        }
        
        float average_temperature = sum_of_temperatures / TEMPERATURE_SENSOR_COUNT;
        save_parameter(NVS_KEY_TEMPERATURE, &average_temperature, sizeof(float));

        //ESP_LOGI(TAG, "Saved temperature: %.2f°C", average_temperature);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

