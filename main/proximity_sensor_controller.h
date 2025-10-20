#include <stdint.h>
#include <stdbool.h>

#ifndef PROXIMITY_SENSOR_CONTROLLER_H
#define PROXIMITY_SENSOR_CONTROLLER_H

typedef enum {
    LOW = 0,
    HIGH = 1
} ProximityThresholdType;

void initialize_proximity_sensors(bool hp_prox_sensor_exist, bool lp_prox_sensor_exist);
void request_excess_status(bool is_lp);
void check_interrupt_status(uint8_t status, bool is_lp);
void read_proximity_of_sensors();
void proximity_read_task(void *pvParameters);
#endif 