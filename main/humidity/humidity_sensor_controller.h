#ifndef HUMIDITY_SENSOR_CONTROLLER_H
#define HUMIDITY_SENSOR_CONTROLLER_H

#include <stdint.h>

void register_humidity_update(void (*callback)(uint8_t));

void initialize_humidity_sensor();

void humidity_read_task(void *param);

uint8_t get_humidity();

uint8_t measure_and_get_humidity();

#endif