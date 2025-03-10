#include <stdint.h>
#include <stdbool.h>

#ifndef PROXIMITY_SENSOR_CONTROL_H
#define PROXIMITY_SENSOR_CONTROL_H

#define VCNL_3020_ADDRESS  0x13

typedef enum {
    LOW = 0,
    HIGH = 1
} ProximityThresholdType;

void initialize_proximity_sensors();
void request_excess_status(uint8_t asserted_sensor_index);
void check_interrupt_status(uint8_t status, bool is_lp);
void log_proximity(uint8_t sensor_index);
#endif 