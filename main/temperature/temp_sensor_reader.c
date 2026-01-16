#include "temp_sensor_reader.h"
#include "temp_sensor_reader_internal.h"

#include "temp_measurement_setter.h"
#include "temp_alert_setter.h"

static uint8_t s_last_temp_byte = 0xFF;
static uint8_t s_alerted_mask   = 0x00;

void temp_sensor_reader_internal_reset(void)
{
    s_last_temp_byte = 0xFF;
    s_alerted_mask = 0x00;
}

void temp_sensor_reader_internal_set_cached(uint8_t temp_byte)
{
    s_last_temp_byte = temp_byte;
}

uint8_t temp_sensor_reader_internal_get_cached(void)
{
    return s_last_temp_byte;
}

void temp_sensor_reader_internal_add_alerted(uint8_t sensor_index)
{
    if (sensor_index < 8) s_alerted_mask |= (uint8_t)(1U << sensor_index);
}

void temp_sensor_reader_internal_remove_alerted(uint8_t sensor_index)
{
    if (sensor_index < 8) s_alerted_mask &= (uint8_t)~(1U << sensor_index);
}

uint8_t temp_sensor_reader_measure_and_get_temperature(void)
{
    uint8_t b = temp_measurement_setter_measure_average_temperature_byte();
    s_last_temp_byte = b;
    return b;
}

uint8_t temp_sensor_reader_get_temperature(void)
{
    if (s_last_temp_byte == 0xFF) {
        return temp_sensor_reader_measure_and_get_temperature();
    }
    return s_last_temp_byte;
}

bool temp_sensor_reader_check_alert_status(void)
{
    return temp_alert_setter_check_alert_status();
}

bool temp_sensor_reader_is_any_alerted_sensor(void)
{
    return s_alerted_mask != 0x00;
}
