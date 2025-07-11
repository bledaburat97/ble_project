#include <stdint.h>

#ifndef GENERAL_MANAGER_H
#define GENERAL_MANAGER_H

void change_helmet_state(bool helmet_state);
void throw_alert_for_temperature(uint8_t sensor_index);
void init_general_manager();
void start_device();
#endif