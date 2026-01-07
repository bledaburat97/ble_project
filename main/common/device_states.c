#include "device_states.h"

const char* get_device_state_str(DeviceState state) {
    switch (state) {
        case STATE_TEMPERATURE_ALERT: return "TemperatureAlert";
        case STATE_HUMIDITY_ALERT:    return "HumidityAlert";
        case STATE_ACTIVE:            return "Active";
        case STATE_INACTIVE:          return "Inactive";
        case STATE_IDLE:              return "Idle";
        default:                      return "Invalid";
    }
}
