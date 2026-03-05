#ifndef HUMIDITY_SENSOR_CONFIG_H
#define HUMIDITY_SENSOR_CONFIG_H

#include <stdint.h>

// HDC1080 ölçüm modu.
typedef enum {
    HDC1080_MODE_SINGLE = 0,
    HDC1080_MODE_BOTH   = 1
} HDC1080_Mode;

// HDC1080 sıcaklık çözünürlüğü.
typedef enum {
    HDC1080_TRES_14BIT = 0,
    HDC1080_TRES_11BIT = 1
} HDC1080_TemperatureResolution;

// HDC1080 nem çözünürlüğü.
typedef enum {
    HDC1080_HRES_14BIT = 0b00,
    HDC1080_HRES_11BIT = 0b01,
    HDC1080_HRES_8BIT  = 0b10
} HDC1080_HumidityResolution;

// HDC1080 config register bit alanları (LSB -> MSB).
typedef struct {
    uint16_t reserved_low  : 8;  // 0-7, 0 kalmalı
    HDC1080_HumidityResolution hres : 2; // 8-9
    HDC1080_TemperatureResolution tres : 1; // 10
    uint16_t btst          : 1;  // 11, sadece okuma
    HDC1080_Mode mode      : 1;  // 12
    uint16_t heat          : 1;  // 13, heater
    uint16_t reserved_high : 1;  // 14, 0 kalmalı
    uint16_t rst           : 1;  // 15, soft reset
} HDC1080_ConfigReg;

#endif
