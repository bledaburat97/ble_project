#include <stdint.h>

#ifndef TEMPERATURE_SENSOR_CONTROLLER_H
#define TEMPERATURE_SENSOR_CONTROLLER_H

void initialize_temperature_sensor();
uint8_t log_temperature();
void temperature_read_task(void *param);
uint8_t get_temperature();
void register_temperature_update(void (*callback)(uint8_t));
void register_temp_alert_callback(void (*callback)(uint8_t));
#endif 