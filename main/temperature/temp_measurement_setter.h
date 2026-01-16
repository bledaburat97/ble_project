#ifndef TEMP_MEASUREMENT_SETTER_H
#define TEMP_MEASUREMENT_SETTER_H

#include "temp_sensor_config.h"

#include <stdint.h>
#include <stdbool.h>

void temp_measurement_setter_initialize_sensors(AlertPolarity alert_polarity);

uint8_t temp_measurement_setter_convert_float_to_byte(float temperature);

float temp_measurement_setter_measure_average_temperature(void);
uint8_t temp_measurement_setter_measure_average_temperature_byte(void);

void temp_measurement_setter_set_normal_thresholds(uint8_t sensor_index);
void temp_measurement_setter_set_alert_thresholds(uint8_t sensor_index);

#endif