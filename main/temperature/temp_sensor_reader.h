#ifndef TEMP_SENSOR_READER_H
#define TEMP_SENSOR_READER_H

#include <stdint.h>
#include <stdbool.h>

uint8_t temp_sensor_reader_measure_and_get_temperature(void);
uint8_t temp_sensor_reader_get_temperature(void);

bool temp_sensor_reader_check_alert_status(void);
bool temp_sensor_reader_is_any_alerted_sensor(void);

#endif
