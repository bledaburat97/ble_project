#include "humidity_sensor.h"

#include "../i2c/i2c_control.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <math.h>

static const char *TAG = "HumiditySensor";
static const float RAW_MAX = 65536.0f;  // 2^16

// HDC1080 ham veriyi °C ve %RH'e çevirir.

// Ham sıcaklık -> °C.
static float convert_raw_temp(uint16_t raw)
{
    return ((float)raw / RAW_MAX) * 165.0f - 40.0f;
}

static float convert_raw_humidity(uint16_t raw)
{
    return ((float)raw / RAW_MAX) * 100.0f;
}

// Ölçüm tetiklmek için yazar.
static esp_err_t write_for_measurement(uint8_t device_address, uint8_t reg_address)
{
    esp_err_t err = i2c_master_write_to_device(
        I2C_FIRST_MASTER_NUM,
        device_address,
        &reg_address,
        1,
        pdMS_TO_TICKS(100)
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to write pointer 0x%02X to device 0x%02X, err=0x%x",
                 reg_address,
                 device_address,
                 err);
    }

    return err;
}

// HDC1080 konfigürasyon register'ını yazar.
void hdc1080_set_configuration(uint8_t device_address, HDC1080_ConfigReg config)
{
    ESP_LOGI(TAG, "Set configuration for HDC1080 at addr: 0x%02X", device_address);

    // Bitfield yapısını 16-bit ham değere çevir
    uint16_t config_value = *(uint16_t *)&config;

    uint8_t buf[2];
    buf[0] = (uint8_t)(config_value >> 8);
    buf[1] = (uint8_t)(config_value & 0xFF);

    write_register(device_address, HDC1080_CONFIG_REG, buf, 2);

    uint8_t readback[2] = {0};
    (void)read_register(device_address, HDC1080_CONFIG_REG, readback, 2);
}

// T+RH ölçümünü tetikler.
void hdc1080_trigger_measurement_trh(uint8_t device_address)
{
    write_for_measurement(device_address, HDC1080_TEMPERATURE_REG);
}

// T+RH okur.
void hdc1080_read_temperature_humidity(
        uint8_t device_address,
        float *temperature_c,
        float *humidity_rh)
{
    uint8_t buf[4] = {0};

    esp_err_t ret = i2c_master_read_from_device(
        I2C_FIRST_MASTER_NUM,
        device_address,
        buf,
        sizeof(buf),
        pdMS_TO_TICKS(50)
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read T+RH from HDC1080, err=0x%x", ret);
        return;
    }

    uint16_t raw_temp = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_rh   = ((uint16_t)buf[2] << 8) | buf[3];

    float t = convert_raw_temp(raw_temp);
    float h = convert_raw_humidity(raw_rh);

    if (temperature_c) {
        *temperature_c = t;
    }
    if (humidity_rh) {
        *humidity_rh = h;
    }

}

// ölçüm tetikler ve sadece nemi döner.
float hdc1080_read_humidity(uint8_t device_address)
{
    float temp_dummy = 0.0f;
    float rh         = 0.0f;

    hdc1080_trigger_measurement_trh(device_address);
    vTaskDelay(pdMS_TO_TICKS(20));
    hdc1080_read_temperature_humidity(device_address, &temp_dummy, &rh);
    return rh;
}

