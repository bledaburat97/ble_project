#include <stdint.h>

#ifndef TEMPERATURE_SENSOR_CONTROL_H
#define TEMPERATURE_SENSOR_CONTROL_H

void initialize_temperature_sensor();
void get_temperature_of_all_sensors(uint8_t* temperature_measurements);
void get_temperature_of_sensor(uint8_t* temperature, uint8_t sensor_index);
float read_temperature_from_nvs();
void temperature_update_task(void *param);
#endif 