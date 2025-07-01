#include <stdbool.h>

#define MAX_STATE_LISTENERS 5

#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

typedef enum {
    STATE_TEMPERATURE_ALERT,
    STATE_HUMIDITY_ALERT,
    STATE_ACTIVE,
    STATE_INACTIVE,
    STATE_START,
    STATE_IDLE
} DeviceState;

typedef struct {
    DeviceState new_state;
} DeferredStateParam;

typedef void (*state_change_callback)(DeviceState device_state);
typedef void (*helmet_state_change_callback)(bool helmet_state);

const char* get_device_state_str(DeviceState state);
void init_state_manager();
void set_device_state(DeviceState device_state);
void set_helmet_state(bool helmet_state);
DeviceState get_device_state();
bool get_helmet_state();
void register_state_change_callback(state_change_callback callback);
void register_helmet_state_change_callback(helmet_state_change_callback callback);
void set_device_state_deferred(DeviceState new_state);
#endif 