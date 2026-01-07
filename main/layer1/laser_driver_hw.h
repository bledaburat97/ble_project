#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LP5036 (veya benzer) lazer/LED driver için donanım soyutlama katmanı.
 * 
 * Buradaki fonksiyonlar:
 * - I2C üzerinden register’lara yazma
 * - Kanal bazlı PWM ayarı
 * - Global enable / shutdown kontrolü
 * yapar; bölge (region) ve terapi gibi kavramlardan habersizdir.
 */

/** Driver’ın I2C adresi (device_configuration.h / kendi config’inden al). */
#ifndef LASER_DRIVER_I2C_ADDRESS
#define LASER_DRIVER_I2C_ADDRESS   0x30  // ÖRNEK, sen gerçek değeri koy
#endif

/** Kullanılan toplam kanal sayısı (LP5036 → 36). */
#ifndef LASER_DRIVER_CHANNEL_COUNT
#define LASER_DRIVER_CHANNEL_COUNT 36
#endif

/**
 * Donanım driver’ı başlat.
 * - I2C bus’in zaten init edildiğini varsayar (i2c_bus_init_main()).
 * - LP5036 global config register’larını default değerlere çeker.
 */
esp_err_t laser_hw_init(void);

/**
 * Tek bir kanalın parlaklığını ayarla.
 * 
 * @param index  0..(LASER_DRIVER_CHANNEL_COUNT-1)
 * @param pwm_value      0..255 (0 = kapalı, 255 = maksimum duty)
 */
esp_err_t laser_hw_set_channel_brightness(uint8_t laser_driver_address,
                                          uint8_t index,
                                          uint8_t pwm_value);

/**
 * Global enable / shutdown kontrolü.
 * 
 * @param enabled  true → tüm kanal çıkışlarını aktif et
 *                 false → tüm kanal çıkışlarını donanımsal olarak kapat
 */
esp_err_t laser_hw_set_global_enable(bool enabled);

#ifdef __cplusplus
}
#endif
