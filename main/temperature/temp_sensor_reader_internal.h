#ifndef TEMP_SENSOR_READER_INTERNAL_H
#define TEMP_SENSOR_READER_INTERNAL_H

#include <stdint.h>

void temp_sensor_reader_internal_reset(void);
void temp_sensor_reader_internal_set_cached(uint8_t temp_byte);

void temp_sensor_reader_internal_add_alerted(uint8_t sensor_index);
void temp_sensor_reader_internal_remove_alerted(uint8_t sensor_index);

uint8_t temp_sensor_reader_internal_get_cached(void);

#endif