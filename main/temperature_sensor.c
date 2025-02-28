#include "temperature_sensor.h"

#include "i2c_control.h"
#include "math.h"
#include "esp_log.h"

#define SENSOR_I2C_ADDR  0x48  // Sensörün I2C adresi
#define TEMPERATURE_REG 0x00
#define CONFIG_REG  0x01
#define LOW_THRESHOLD_REG 0x02
#define HIGH_THRESHOLD_REG 0x03

#define TEMPERATURE_RESOLUTION 0.0625

static const char *TAG = "TemperatureSensor";
static uint8_t i2c_master_num;
static void convert_threshold_to_bytes(float thresholdInDegree, uint8_t *high_byte, uint8_t *low_byte) {
    if (!high_byte || !low_byte) {
        return;
    }

    *high_byte = (uint8_t)thresholdInDegree;

    float fractional_part = thresholdInDegree - (int)thresholdInDegree;
    uint8_t fraction_bits = (uint8_t)round(fractional_part / TEMPERATURE_RESOLUTION); 

    *low_byte = (fraction_bits & 0x0F) << 4;  
}

static float convert_bytes_to_threshold(uint8_t high_byte, uint8_t low_byte) {
    float integer_part = (float)high_byte;

    uint8_t fraction_bits = (low_byte >> 4) & 0x0F;

    float fractional_part = fraction_bits * TEMPERATURE_RESOLUTION;

    return integer_part + fractional_part;
}

void set_i2c_master_num(uint8_t master_num) {
    i2c_master_num = master_num;
}

void set_configuration(uint8_t device_address, TempSensorConfigReg config) {
    uint8_t config_byte = *(uint8_t*)&config;
    ESP_LOGI(TAG, "Configuration's written byte: %u°C", config_byte);

    write_register(device_address, CONFIG_REG, &config_byte, 1, i2c_master_num);
    //lp_core_send_write_command(device_address, CONFIG_REG, &config_byte, 1);
    uint8_t configurationBytes[2];
    read_register(device_address, CONFIG_REG, configurationBytes, 2, i2c_master_num);
    //lp_core_send_read_command(device_address, CONFIG_REG, configurationBytes, 2);

    ESP_LOGI(TAG, "Configuration's read byte: %u°C", configurationBytes[0]);
}

void set_threshold_temperature(uint8_t device_address, float thresholdInDegree, TemperatureThresholdType type) {
    uint8_t msb, lsb;

    convert_threshold_to_bytes(thresholdInDegree, &msb, &lsb);
    uint8_t threshold[2] ={msb, lsb}; 
    if (type == LOW){
        write_register(device_address, LOW_THRESHOLD_REG, threshold, 2, i2c_master_num);
        //lp_core_send_write_command(device_address, LOW_THRESHOLD_REG, threshold, 2);
    }
    else if(type == HIGH) {
        write_register(device_address, HIGH_THRESHOLD_REG, threshold, 2, i2c_master_num);
        //lp_core_send_write_command(device_address, HIGH_THRESHOLD_REG, threshold, 2);
    }
}

void log_current_temperature(uint8_t device_address) {
    uint8_t temperatureBytes[2];
    read_register(device_address, TEMPERATURE_REG, temperatureBytes, 2, i2c_master_num);
    //lp_core_send_read_command(device_address, TEMPERATURE_REG, temperatureBytes, 2);
    ESP_LOGI(TAG, "Temperature's first byte: %u°C", temperatureBytes[0]);
    ESP_LOGI(TAG, "Temperature's second byte: %u°C", temperatureBytes[1]);
}

float read_temperature_of_sensor(uint8_t device_address) {
    uint8_t temperatureBytes[2];
    read_register(device_address, TEMPERATURE_REG, temperatureBytes, 2, i2c_master_num);
    //lp_core_send_read_command(device_address, TEMPERATURE_REG, temperatureBytes, 2);
    ESP_LOGI(TAG, "Temperature's first byte: %u", temperatureBytes[0]);
    ESP_LOGI(TAG, "Temperature's second byte: %u", temperatureBytes[1]);
    ESP_LOGI(TAG, "---------------------------");
    float temperature = convert_bytes_to_threshold(temperatureBytes[0], temperatureBytes[1]);
    ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
    return temperature;
}
