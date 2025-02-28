#include "i2c_control.h"
#include "temperature_sensor_control.h"
#include "esp_log.h"
#include "storage_management.h"
#include "temperature_sensor.h"
#include "esp_task_wdt.h"
#include "temperature_alarm_control.h"

#define FIRST_PJ85775_ADDRESS 0x48   // I2C address for the first LP5036
#define SECOND_PJ85775_ADDRESS 0x49   // I2C address for the first LP5036
#define THIRD_PJ85775_ADDRESS 0x4A   // I2C address for the first LP5036

#define NVS_KEY_TEMPERATURE "last_temp"

static const char *TAG = "TemperatureControl";
static const uint8_t sensor_addresses[] = {FIRST_PJ85775_ADDRESS, SECOND_PJ85775_ADDRESS, THIRD_PJ85775_ADDRESS};

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
    set_i2c_master_num(I2C_FIRST_MASTER_NUM);
    set_configuration(sensor_addresses[0], config);
    set_threshold_temperature(sensor_addresses[0], 25.5, LOW);
    set_threshold_temperature(sensor_addresses[0], 27, HIGH);

    float last_stored_temp;
    if (read_parameter(NVS_KEY_TEMPERATURE, &last_stored_temp, sizeof(float)) == ESP_OK) {
        ESP_LOGI(TAG, "Last saved temperature: %.2f°C", last_stored_temp);
    } else {
        ESP_LOGE(TAG, "Last saved temperature can not be got.");
    }
}

void log_temperature() {
    ESP_LOGI(TAG, "Log temperature.");
    float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[0]);
    save_parameter(NVS_KEY_TEMPERATURE, &temperatureInDegree, sizeof(float));
}

void temperature_update_task(void *param) {
    esp_task_wdt_add(NULL);  // Task’ı Watchdog’a ekleyin

    while (1) {
        float sum_of_temperatures = 0;

        float temperatureInDegree = read_temperature_of_sensor(sensor_addresses[0]);
        sum_of_temperatures += temperatureInDegree;
        
        float average_temperature = sum_of_temperatures;
        save_parameter(NVS_KEY_TEMPERATURE, &average_temperature, sizeof(float));

        ESP_LOGI(TAG, "Saved temperature: %.2f°C", average_temperature);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

