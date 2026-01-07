#include "temp_sensor_hw.h"

#include "i2c_bus.h"
#include "esp_log.h"
#include <math.h>

#define TEMP_REG            0x00
#define TEMP_CONFIG_REG     0x01
#define TEMP_LOW_TH_REG     0x02
#define TEMP_HIGH_TH_REG    0x03

#define TEMP_RESOLUTION     0.0625f

static const char *TAG = "TempSensorHW";

static void convert_threshold_to_bytes(float threshold_in_degree,
                                       uint8_t *high_byte,
                                       uint8_t *low_byte)
{
    if (!high_byte || !low_byte) {
        return;
    }

    *high_byte = (uint8_t)threshold_in_degree;

    float fractional_part = threshold_in_degree - (int)threshold_in_degree;
    uint8_t fraction_bits = (uint8_t)lroundf(fractional_part / TEMP_RESOLUTION);

    *low_byte = (uint8_t)((fraction_bits & 0x0F) << 4);
}

static float convert_bytes_to_celsius(uint8_t high_byte, uint8_t low_byte)
{
    float integer_part   = (float)high_byte;
    uint8_t fraction_bits = (uint8_t)((low_byte >> 4) & 0x0F);
    float fractional_part = (float)fraction_bits * TEMP_RESOLUTION;
    return integer_part + fractional_part;
}

esp_err_t temp_hw_set_config(uint8_t device_address,
                             TempSensorConfigReg config)
{
    uint8_t config_byte = *(uint8_t *)&config;
    esp_err_t err = i2c_bus_write_reg(device_address,
                                      TEMP_CONFIG_REG,
                                      &config_byte,
                                      1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write temp config (dev=0x%02X)", device_address);
        return err;
    }

    return ESP_OK;
}

esp_err_t temp_hw_set_threshold(uint8_t device_address,
                                float threshold_in_degree,
                                TempThresholdType type)
{
    uint8_t msb = 0;
    uint8_t lsb = 0;

    convert_threshold_to_bytes(threshold_in_degree, &msb, &lsb);
    uint8_t threshold_bytes[2] = { msb, lsb };

    uint8_t reg = (type == TEMP_THRESHOLD_LOW)
                  ? TEMP_LOW_TH_REG
                  : TEMP_HIGH_TH_REG;

    esp_err_t err = i2c_bus_write_reg(device_address, reg, threshold_bytes, 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to write temp threshold (dev=0x%02X, type=%d, value=%.2f)",
                 device_address, (int)type, threshold_in_degree);
        return err;
    }

    return ESP_OK;
}

esp_err_t temp_hw_read_celsius(uint8_t device_address,
                               float *out_temperature)
{
    if (!out_temperature) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t temp_bytes[2] = { 0 };
    esp_err_t err = i2c_bus_read_reg(device_address,
                                     TEMP_REG,
                                     temp_bytes,
                                     2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature (dev=0x%02X)", device_address);
        return err;
    }

    *out_temperature = convert_bytes_to_celsius(temp_bytes[0], temp_bytes[1]);
    return ESP_OK;
}
