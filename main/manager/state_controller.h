#ifndef STATE_CONTROLLER_H
#define STATE_CONTROLLER_H

#include <stdbool.h>
#include "../common/device_states.h"

void set_device_state(DeviceState new_state);
DeviceState get_device_state();
void init_state_controller();
#endif