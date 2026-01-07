#ifndef DEVICE_STATES_H
#define DEVICE_STATES_H

typedef enum {
    STATE_TEMPERATURE_ALERT,
    STATE_HUMIDITY_ALERT,
    STATE_ACTIVE,
    STATE_INACTIVE,
    STATE_IDLE
} DeviceState;

const char* get_device_state_str(DeviceState state);
#endif