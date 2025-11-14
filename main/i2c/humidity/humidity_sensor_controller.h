#ifndef HUMIDITY_SENSOR_CONTROLLER_H
#define HUMIDITY_SENSOR_CONTROLLER_H

#include <stdint.h>

// Nem ölçümü callback'i: 0-100% arası 1 byte'a quantize edilmiş değer
void register_humidity_update(void (*callback)(uint8_t));

// Sensörü ve config'i başlat
void initialize_humidity_sensor();

// FreeRTOS task fonksiyonu (xTaskCreate ile kullan)
void humidity_read_task(void *param);

// İstediğin zaman anlık byte formatında nem
uint8_t get_humidity();

uint8_t measure_and_get_humidity();

#endif