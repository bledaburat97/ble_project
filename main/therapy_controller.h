#include <stdbool.h>


#ifndef THERAPY_CONTROLLER_H
#define THERAPY_CONTROLLER_H

void init_device_param_status();
void set_helmet_status(bool status);
bool get_helmet_status();
void set_temperature_status(bool status);
bool get_temperature_status();
void set_therapy_status(bool status);
bool get_therapy_status();
void set_lasers_status(bool status);
bool get_lasers_status();
bool can_therapy_start();
#endif 