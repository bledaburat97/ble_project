#include <stdint.h>

#ifndef TEMPERATURE_SENSOR_CONTROL_H
#define TEMPERATURE_SENSOR_CONTROL_H

void initialize_temperature_sensor();
uint8_t log_temperature();
void temperature_update_task(void *param);

#endif 