#include <stdbool.h>

#define MAX_STATE_LISTENERS 5

#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

typedef enum {
    STATE_TEMPERATURE_ALERT,
    STATE_HUMIDITY_ALERT,
    STATE_ACTIVE,
    STATE_INACTIVE,
    STATE_IDLE
} DeviceState;

typedef void (*state_change_callback)(DeviceState device_state);

const char* get_device_state_str(DeviceState state);
void init_state_manager();
void set_device_state(DeviceState device_state);
bool set_helmet_state(bool helmet_state);
DeviceState get_device_state();
bool get_helmet_state();
void register_state_change_callback(state_change_callback callback);
#endif 