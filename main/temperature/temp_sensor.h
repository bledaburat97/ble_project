#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <stdint.h>
#include "temp_sensor_config.h"

typedef enum {
    LOW = 0,
    HIGH = 1
} TemperatureThresholdType;

void temp_sensor_set_configuration(uint8_t device_address, TempSensorConfigReg config);
void temp_sensor_set_threshold(uint8_t device_address, float threshold_in_degree, TemperatureThresholdType type);
float temp_sensor_read(uint8_t device_address);
#endif