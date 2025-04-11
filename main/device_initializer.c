
#include "device_initializer.h"
#include "state_manager.h"
#include "temperature_alarm_control.h"
#include "deep_sleep_manager.h"
#include "timer_management.h"

static void on_state_changed(DeviceState new_state){
    if(new_state == STATE_START) {
        if(check_alert_status()) {
            enter_deep_sleep();
        }
        else{
            start_inactivity_timer();
        }
    }
}

void init_device_initializer()
{
    register_state_change_callback(on_state_changed);
}