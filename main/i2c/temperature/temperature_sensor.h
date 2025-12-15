#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>
#include "temperature_sensor_config.h"

typedef enum {
    LOW = 0,
    HIGH = 1
} TemperatureThresholdType;

void set_configuration(uint8_t device_address, TempSensorConfigReg config);
void set_threshold_temperature(uint8_t device_address, float threshold_in_degree, TemperatureThresholdType type);
float read_temperature_of_sensor(uint8_t device_address);
#endif