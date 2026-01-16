#include "temp_measurement_setter.h"

#include "temp_sensor_config.h"
#include "temp_sensor.h"
#include "temp_sensor_config.h"

#include "esp_log.h"
#include <math.h>

static const char *TAG = "TempMeasurementSetter";

const uint8_t TEMP_SENSOR_ADDRESS_LIST[TEMPERATURE_SENSOR_COUNT] = {
    SECOND_PJ85775_ADDRESS,
    FIRST_PJ85775_ADDRESS,
    THIRD_PJ85775_ADDRESS
};

uint8_t temp_measurement_setter_convert_float_to_byte(float temperature)
{
    if (temperature < 0.0f || temperature > 64.0f) return 0xFF;

    uint8_t integer_part = (uint8_t)temperature;
    float fractional_part = temperature - (float)integer_part;

    uint8_t frac_bits;
    if (fractional_part < 0.25f)      frac_bits = 0b00;
    else if (fractional_part < 0.5f)  frac_bits = 0b01;
    else if (fractional_part < 0.75f) frac_bits = 0b10;
    else                              frac_bits = 0b11;

    return (uint8_t)((integer_part << 2) | frac_bits);
}

static TempSensorConfigReg build_default_config(AlertPolarity alert_polarity)
{
    TempSensorConfigReg config;
    config.os   = OS_DISABLED;
    config.cr   = CR_4HZ;
    config.fq   = FAULT_QUEUE_2;
    config.pol  = alert_polarity;
    config.altm = ALERT_COMPARATOR_MODE;
    config.sd   = NORMAL_MODE;
    return config;
}

void temp_measurement_setter_set_normal_thresholds(uint8_t sensor_index)
{
    if (sensor_index >= TEMPERATURE_SENSOR_COUNT) return;

    uint8_t addr = TEMP_SENSOR_ADDRESS_LIST[sensor_index];
    temp_sensor_set_threshold(addr, LOW_THRESHOLD_IN_NORMAL, LOW);
    temp_sensor_set_threshold(addr, HIGH_THRESHOLD_IN_NORMAL, HIGH);
}

void temp_measurement_setter_set_alert_thresholds(uint8_t sensor_index)
{
    if (sensor_index >= TEMPERATURE_SENSOR_COUNT) return;

    uint8_t addr = TEMP_SENSOR_ADDRESS_LIST[sensor_index];
    temp_sensor_set_threshold(addr, LOW_THRESHOLD_IN_ALERT, LOW);
    temp_sensor_set_threshold(addr, HIGH_THRESHOLD_IN_ALERT, HIGH);
}

void temp_measurement_setter_initialize_sensors(AlertPolarity alert_polarity)
{
    ESP_LOGI(TAG, "Initialize temp sensors (I2C config + normal thresholds)");

    TempSensorConfigReg config = build_default_config(alert_polarity);

    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        uint8_t addr = TEMP_SENSOR_ADDRESS_LIST[i];
        temp_sensor_set_configuration(addr, config);
        temp_measurement_setter_set_normal_thresholds((uint8_t)i);
    }
}

float temp_measurement_setter_measure_average_temperature(void)
{
    float sum = 0.0f;
    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        sum += temp_sensor_read(TEMP_SENSOR_ADDRESS_LIST[i]);
    }
    return sum / (float)TEMPERATURE_SENSOR_COUNT;
}

uint8_t temp_measurement_setter_measure_average_temperature_byte(void)
{
    float avg = temp_measurement_setter_measure_average_temperature();
    return temp_measurement_setter_convert_float_to_byte(avg);
}