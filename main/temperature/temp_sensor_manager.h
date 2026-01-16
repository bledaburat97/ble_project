#ifndef TEMP_SENSOR_MANAGER_H
#define TEMP_SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void temp_sensor_manager_initialize(void);

void temp_sensor_manager_register_temperature_update(void (*cb)(uint8_t temp_byte));
void temp_sensor_manager_register_temp_alert(void (*cb)(uint8_t sensor_index));

void temp_sensor_manager_temperature_read_task(void *param);
void temp_sensor_manager_alert_monitor_task(void *param);

#endif 
