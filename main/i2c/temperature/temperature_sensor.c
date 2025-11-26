#include "temperature_sensor.h"

#include "../i2c_control.h"

#include "../../lp_core/lp_core_queue_manager.h"

#include "math.h"
#include "esp_log.h"

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

static float convert_bytes_to_float(uint8_t high_byte, uint8_t low_byte) {
    float integer_part = (float)high_byte;

    uint8_t fraction_bits = (low_byte >> 4) & 0x0F;

    float fractional_part = fraction_bits * TEMPERATURE_RESOLUTION;

    return integer_part + fractional_part;
}

static void add_lp_read_command_to_queue(uint8_t device_address, uint8_t reg_address, size_t length) {
    uint32_t lp_core_device_address = 0x00000000 | (device_address & 0xFF);
    uint32_t lp_core_register =  0x00000000 | (reg_address & 0xFF);
    uint32_t lp_core_byte_count = length;
    uint32_t lp_core_command = 2;
    uint32_t lp_core_value = 0;

    ESP_LOGI(TAG, "Queue'ya read ekle: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu", 
        lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);

    queue_add_task(lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);
}

static void add_lp_write_command_to_queue(uint8_t device_address, uint8_t reg_address, uint8_t* data, size_t length) {
    uint32_t lp_core_device_address = 0x00000000 | (device_address & 0xFF);
    uint32_t lp_core_register =  0x00000000 | (reg_address & 0xFF);
    uint32_t lp_core_byte_count = length;
    uint32_t lp_core_command = 1;
    uint32_t lp_core_value = 0;
    if(length == 1) {
        lp_core_value = lp_core_value | (data[0] & 0xFF);
    }
    else if(length == 2) {
        lp_core_value = lp_core_value | ((data[0] << 8) | data[1]);
    }
    ESP_LOGI(TAG, "Queue'ya yaz: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu", 
        lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);

    queue_add_task(lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);
}

void set_i2c_master_num(uint8_t master_num) {
    i2c_master_num = master_num;
}

void set_configuration(uint8_t device_address, TempSensorConfigReg config) {
    //ESP_LOGI(TAG, "Set configuration for temperature sensor of device address: %u", device_address);

    uint8_t config_byte = *(uint8_t*)&config;
    //ESP_LOGI(TAG, "Configuration's written byte: %u°C", config_byte);

    write_register(device_address, CONFIG_REG, &config_byte, 1, i2c_master_num);

    //-LP-//add_lp_write_command_to_queue(device_address, CONFIG_REG, &config_byte, 1);
    uint8_t configurationBytes[2];
    read_register(device_address, CONFIG_REG, configurationBytes, 2, i2c_master_num);
    //-LP-//add_lp_read_command_to_queue(device_address, CONFIG_REG, 2);

    //ESP_LOGI(TAG, "Configuration's read byte: %u°C", configurationBytes[0]);
}

void set_threshold_temperature(uint8_t device_address, float thresholdInDegree, TemperatureThresholdType type) {
    uint8_t msb, lsb;

    convert_threshold_to_bytes(thresholdInDegree, &msb, &lsb);
    uint8_t threshold[2] ={msb, lsb}; 
    if (type == LOW){
        ESP_LOGI(TAG, "New low temperature limit: %.2f°C", thresholdInDegree);
        write_register(device_address, LOW_THRESHOLD_REG, threshold, 2, i2c_master_num);
        //-LP-//add_lp_write_command_to_queue(device_address, LOW_THRESHOLD_REG, threshold, 2);
    }
    else if(type == HIGH) {
        ESP_LOGI(TAG, "New high temperature limit: %.2f°C", thresholdInDegree);
        write_register(device_address, HIGH_THRESHOLD_REG, threshold, 2, i2c_master_num);
        //-LP-//add_lp_write_command_to_queue(device_address, HIGH_THRESHOLD_REG, threshold, 2);
    }
}

float read_temperature_of_sensor(uint8_t device_address) {
    //ESP_LOGI(TAG, "Read temperature from temperature sensor of device address: %u", device_address);

    uint8_t temperatureBytes[2];
    read_register(device_address, TEMPERATURE_REG, temperatureBytes, 2, i2c_master_num);

    //ESP_LOGI(TAG, "1st byte read temperature: %u", temperatureBytes[0]);
    //ESP_LOGI(TAG, "2nd byte read temperature: %u", temperatureBytes[1]);

    //-LP-//add_lp_read_command_to_queue(device_address, TEMPERATURE_REG, 2);
    float temperature = convert_bytes_to_float(temperatureBytes[0], temperatureBytes[1]);
    //ESP_LOGI(TAG, "Temperature: %.2f°C", temperature);
    return temperature;
}