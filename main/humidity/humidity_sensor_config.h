#ifndef HUMIDITY_SENSOR_CONFIG_H
#define HUMIDITY_SENSOR_CONFIG_H

#include <stdint.h>

/**
 * @brief HDC1080 acquisition mode selection.
 */
typedef enum {
    HDC1080_MODE_SINGLE = 0,
    HDC1080_MODE_BOTH   = 1
} HDC1080_Mode;

/**
 * @brief HDC1080 temperature resolution.
 */
typedef enum {
    HDC1080_TRES_14BIT = 0,
    HDC1080_TRES_11BIT = 1
} HDC1080_TemperatureResolution;

/**
 * @brief HDC1080 humidity resolution.
 */
typedef enum {
    HDC1080_HRES_14BIT = 0b00,
    HDC1080_HRES_11BIT = 0b01,
    HDC1080_HRES_8BIT  = 0b10
} HDC1080_HumidityResolution;

/**
 * @brief HDC1080 configuration register bitfield.
 *
 * Bitfield sırası, ESP-IDF'nin (GCC, little-endian) struct bitfield layout'una
 * göre LSB'den MSB'ye doğru tanımlanmıştır (temperature sensöründeki struct ile uyumlu).
 */
typedef struct {
    uint16_t reserved_low  : 8;  ///< bit 0-7, must be 0
    HDC1080_HumidityResolution hres : 2; ///< bit 8-9
    HDC1080_TemperatureResolution tres : 1; ///< bit 10
    uint16_t btst          : 1;  ///< bit 11, read-only, write 0
    HDC1080_Mode mode      : 1;  ///< bit 12
    uint16_t heat          : 1;  ///< bit 13, 0: heater off
    uint16_t reserved_high : 1;  ///< bit 14, must be 0
    uint16_t rst           : 1;  ///< bit 15, write 1 for soft reset
} HDC1080_ConfigReg;

#endif /* HUMIDITY_SENSOR_CONFIG_H */