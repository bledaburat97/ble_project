#include <stdint.h>
#include <stdbool.h>

#ifndef PROXIMITY_SENSOR_CONTROL_H
#define PROXIMITY_SENSOR_CONTROL_H

bool check_threshold_exceeded(uint8_t sensor_index, bool is_high);
bool get_sensor_detection_status(uint8_t sensor_index);
void set_sensor_detection_status(uint8_t sensor_index, bool status);
void set_default_thresholds(uint8_t sensor_index);
void increase_thresholds(uint8_t sensor_index);
void reset_interrupt(uint8_t sensor_index);
bool check_other_sensor_detected(uint8_t asserted_sensor_index);
void set_helmet_status(bool status);
bool get_helmet_status();
void initialize_proximity_sensors();

typedef struct {
    uint8_t rw_bits;      // The 3-bit R/W value
    float measurement_rate; // Corresponding measurement rate in measurements/s
} MeasurementRate;


#endif 