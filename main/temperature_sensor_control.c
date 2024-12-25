#include "i2c_control.h"
#include "temperature_sensor_control.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define FIRST_PJ85775_ADDRESS 0x90   // I2C address for the first LP5036
#define SECOND_PJ85775_ADDRESS 0x91   // I2C address for the first LP5036
#define THIRD_PJ85775_ADDRESS 0x92   // I2C address for the first LP5036

#define REG1 0x01
#define REG2 0x02
#define REG3 0x03
#define REG14 0x0E

#define TEMPERATURE_RESOLUTION 0.625

#define NVS_NAMESPACE "storage"
#define NVS_KEY_TEMPERATURE "last_temp"

static const char *TEMP_TAG = "TemperatureControl";
static const uint8_t sensor_addresses[] = {FIRST_PJ85775_ADDRESS, SECOND_PJ85775_ADDRESS, THIRD_PJ85775_ADDRESS};


void initialize_temperature_sensor() {
    uint8_t configuration_info = 0x2A;     //conversion rate is 0.25, fault queue number as 2
    uint8_t low_threshold[2] = {0x29, 0x00}; // 41 degrees Celsius
    uint8_t high_threshold[2] = {0x2A, 0x00}; // 42 degrees Celsius

    for (int i = 0; i < 3; i++) {
        write_register(sensor_addresses[i], REG1, &configuration_info, 1, I2C_FIRST_MASTER_NUM);
        write_register(sensor_addresses[i], REG2, low_threshold, 2, I2C_FIRST_MASTER_NUM);
        write_register(sensor_addresses[i], REG3, high_threshold, 2, I2C_FIRST_MASTER_NUM);
    }
}

void get_temperature_of_all_sensors(uint8_t* temperature_measurements) {

    for (int i = 0; i < 3; i++) {
        uint8_t temperature[2];
        get_temperature_of_sensor(temperature, i);
        temperature_measurements[i * 2] = temperature[0];
        temperature_measurements[i * 2 + 1] = temperature[1];
    }
}

void get_temperature_of_sensor(uint8_t* temperature, uint8_t sensor_index) {
    read_register(sensor_addresses[sensor_index], REG14, temperature, 2, I2C_FIRST_MASTER_NUM);
}

static float convert_temperature_to_degree(const uint8_t* temperature) {
    uint16_t raw_temp = (temperature[0] << 8) | temperature[1];
    return raw_temp * TEMPERATURE_RESOLUTION;
}

static void save_temperature_to_nvs(float temperature) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS handle in read/write mode
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS");
        return;
    }

    // Write temperature to NVS
    err = nvs_set_blob(nvs_handle, NVS_KEY_TEMPERATURE, &temperature, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to write temperature to NVS");
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to commit changes to NVS");
    }

    // Close NVS handle
    nvs_close(nvs_handle);
}

float read_temperature_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    float temperature = 0.0;

    // Open NVS handle in read-only mode
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Temperature not found in NVS, returning default value");
        return temperature; // Return default if not found
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS");
        return temperature;
    }

    // Read temperature from NVS
    size_t required_size = sizeof(float);
    err = nvs_get_blob(nvs_handle, NVS_KEY_TEMPERATURE, &temperature, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Temperature not found in NVS, returning default value");
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to read temperature from NVS");
    }

    // Close NVS handle
    nvs_close(nvs_handle);

    return temperature;
}

void temperature_update_task(void *param) {
    while (1) {
        float sum_of_temperatures = 0;
        for (int i = 0; i < 3; i++) {
            uint8_t temperature[2];
            get_temperature_of_sensor(temperature, i);
            sum_of_temperatures += convert_temperature_to_degree(temperature);
        }
        float average_temperature = sum_of_temperatures / 3;

        save_temperature_to_nvs(average_temperature);

        ESP_LOGI(TEMP_TAG, "Saved temperature: %.2f°C", average_temperature);

        // Update every 1 second (adjust as needed)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

