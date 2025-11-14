#ifndef HUMIDITY_SENSOR_H
#define HUMIDITY_SENSOR_H

#include <stdint.h>
#include "humidity_sensor_config.h"

#define HDC1080_TEMPERATURE_REG   0x00
#define HDC1080_HUMIDITY_REG      0x01
#define HDC1080_CONFIG_REG        0x02

#define HDC1080_I2C_ADDRESS       0x40  // 7-bit adres (datasheette 1000000b)

// Konfigürasyon yaz (HEAT, MODE, TRES, HRES)
void hdc1080_set_configuration(uint8_t device_address, HDC1080_ConfigReg config);

// Tek seferlik ölçüm tetikle (T+RH)
void hdc1080_trigger_measurement_trh(uint8_t device_address);

// Tek seferde T + RH oku (ölçüm tetiklendikten ve bekleme süresi geçtikten sonra)
void hdc1080_read_temperature_humidity(
        uint8_t device_address,
        float *temperature_c,
        float *humidity_rh
);

// Sadece nem okumak istersen convenience fonksiyonu
float hdc1080_read_humidity(uint8_t device_address);

#endif