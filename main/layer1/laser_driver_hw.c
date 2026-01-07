#include "laser_driver_hw.h"

#include "i2c_bus.h"
#include "esp_log.h"
#include "device_configuration.h"

// Bunları kendi header’larından al:
#include "laser_driver_registers.h"   // Örn: LP5036_DEVICE_CONFIG0, OUT0_PWM, vs.

static const char *TAG = "LaserHW";

// Örnek: LP5036 için bazı temel register adresleri
#ifndef LASER_REG_DEVICE_CONFIG0
#define LASER_REG_DEVICE_CONFIG0   0x00
#endif

#ifndef LASER_REG_DEVICE_CONFIG1
#define LASER_REG_DEVICE_CONFIG1   0x01
#endif

#ifndef LASER_REG_OUT0_PWM
#define LASER_REG_OUT0_PWM         0x14
#endif

/**
 * LP5036’yı default moda al:
 * - Chip enable
 * - Auto-increment
 * - Bank mode vs. (senin kullandığın moda göre)
 */
static esp_err_t laser_hw_write_default_config(void)
{
    esp_err_t err;
    uint8_t value;

    // DEVICE_CONFIG0: Chip enable vb.
    value = 0x40; // ÖRNEK: CHIP_EN=1, diğer bitler default
    err = i2c_bus_write_reg(LASER_DRIVER_I2C_ADDRESS,
                             LASER_REG_DEVICE_CONFIG0,
                             &value,
                             1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write DEVICE_CONFIG0: 0x%x", err);
        return err;
    }

    // DEVICE_CONFIG1: Auto-increment, log scale vs. (kendi config’ine göre)
    value = 0x00; // ÖRNEK
    err = i2c_bus_write_reg(LASER_DRIVER_I2C_ADDRESS,
                             LASER_REG_DEVICE_CONFIG1,
                             &value,
                             1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write DEVICE_CONFIG1: 0x%x", err);
        return err;
    }

    return ESP_OK;
}

esp_err_t laser_hw_init(void)
{
    // Burada I2C bus’ın init olduğunu varsayıyoruz.
    ESP_LOGI(TAG, "Initializing laser driver (I2C=0x%02X).",
             LASER_DRIVER_I2C_ADDRESS);

    esp_err_t err = laser_hw_write_default_config();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Laser driver default config failed: 0x%x", err);
        return err;
    }

    ESP_LOGI(TAG, "Laser driver initialized successfully.");
    return ESP_OK;
}

esp_err_t laser_hw_set_channel_brightness(uint8_t laser_driver_address,
                                          uint8_t index,
                                          uint8_t pwm_value)
{
    uint8_t start_reg = (uint8_t)(LASER_REG_OUT0_PWM + index);
    return i2c_bus_write_reg(laser_driver_address, start_reg, &pwm_value, 1);
}

esp_err_t laser_hw_set_global_enable(bool enabled)
{
    // Örnek: DEVICE_CONFIG0’daki CHIP_EN bitini set/clear et.
    uint8_t value;

    // Burada önce mevcut değeri okumak istersen i2c_bus_read_reg ile okuyup
    // bit manipülasyonu yapabilirsin. Basitlik olsun diye direkt yazıyorum:
    value = enabled ? 0x40 : 0x00;  // ÖRNEK: CHIP_EN=bit6

    esp_err_t err = i2c_bus_write_reg(LASER_DRIVER_I2C_ADDRESS,
                                      LASER_REG_DEVICE_CONFIG0,
                                      &value,
                                      1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set global enable=%d: 0x%x",
                 (int)enabled, err);
    } else {
        ESP_LOGI(TAG, "Laser global %s",
                 enabled ? "ENABLED" : "DISABLED");
    }

    return err;
}
