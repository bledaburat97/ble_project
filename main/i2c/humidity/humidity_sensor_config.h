#ifndef HUMIDITY_SENSOR_CONFIG_H
#define HUMIDITY_SENSOR_CONFIG_H

#include <stdint.h>

// MODE - acquisition mode
typedef enum {
    HDC1080_MODE_SINGLE = 0, // Sadece T veya RH ölçümü (adres pointer'a göre)
    HDC1080_MODE_BOTH   = 1  // Sırasıyla önce T sonra RH ölç (önerilen)
} HDC1080_Mode;

// TRES - Temperature resolution (bit 10)
typedef enum {
    HDC1080_TRES_14BIT = 0,
    HDC1080_TRES_11BIT = 1
} HDC1080_TemperatureResolution;

// HRES - Humidity resolution (bit 9-8)
typedef enum {
    HDC1080_HRES_14BIT = 0b00,
    HDC1080_HRES_11BIT = 0b01,
    HDC1080_HRES_8BIT  = 0b10
} HDC1080_HumidityResolution;

// Bitfield sırası, ESP-IDF'nin (GCC, little-endian) struct bitfield
// layout'ına göre LSB'den MSB'ye doğru tanımlandı.
// (Temperature sensöründeki struct ile aynı mantık.)
typedef struct {
    uint16_t reserved_low  : 8;  // bit 0-7, her zaman 0 olmalı
    HDC1080_HumidityResolution hres : 2; // bit 8-9
    HDC1080_TemperatureResolution tres : 1; // bit 10
    uint16_t btst          : 1;  // bit 11, read-only (0 yaz)
    HDC1080_Mode mode      : 1;  // bit 12
    uint16_t heat          : 1;  // bit 13, 0: heater off
    uint16_t reserved_high : 1;  // bit 14, 0
    uint16_t rst           : 1;  // bit 15, 1 yazarsan soft reset
} HDC1080_ConfigReg;

#endif // HUMIDITY_SENSOR_CONFIG_H