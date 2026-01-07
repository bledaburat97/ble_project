#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TEMP_THRESHOLD_LOW = 0,
    TEMP_THRESHOLD_HIGH
} TempThresholdType;

typedef struct {
    uint8_t raw;   // birebir sensör configuraton register struct’ını
} TempSensorConfigReg; // istersen burada typedef yerine mevcut union/struct’ını kullan

/**
 * Konfigürasyon register'ını yazar.
 */
esp_err_t temp_hw_set_config(uint8_t device_address,
                             TempSensorConfigReg config);

/**
 * Sıcaklık threshold register'larını ayarlar (high / low).
 * threshold_in_degree: derece cinsinden.
 */
esp_err_t temp_hw_set_threshold(uint8_t device_address,
                                float threshold_in_degree,
                                TempThresholdType type);

/**
 * Sensörden anlık sıcaklık okur (°C).
 */
esp_err_t temp_hw_read_celsius(uint8_t device_address,
                               float *out_temperature);

#ifdef __cplusplus
}
#endif
