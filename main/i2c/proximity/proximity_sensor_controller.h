
#ifndef PROXIMITY_SENSOR_CONTROLLER_H
#define PROXIMITY_SENSOR_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LOW = 0,
    HIGH = 1
} ProximityThresholdType;

typedef void (*helmet_state_callback)(bool helmet_on);

void initialize_proximity_sensors(bool hp_prox_sensor_exist, bool lp_prox_sensor_exist, helmet_state_callback callback);
void request_excess_status(bool is_lp);
void check_interrupt_status(uint8_t status, bool is_lp);
void proximity_read_task(void *pvParameters);
bool get_helmet_state();
#endif 