#ifndef DEVICE_INITIATOR_H
#define DEVICE_INITIATOR_H

#include <stdbool.h>

typedef struct {
    // Core features
    bool ble_active;
    bool state_and_timer_active;

    // Proximity sensors
    bool hp_prox_sensor_active;
    bool lp_prox_sensor_active;

    // Storage
    bool therapy_counter_partition_active;
    bool log_partition_active;
    bool log_orchestrator_active;

    // Drivers
    bool laser_and_led_drivers_active;
    bool apply_default_brightness_on_boot;

    // Button / sleep
    bool deep_sleep_button_control_active;

    // Logging policy
    bool creating_logs_permitted;

    // Restore
    bool continue_uncompleted_therapy;

    // Other
    bool mode_indicator_active;
    bool wifi_config_active;
} DeviceFeatureConfig;

void device_initiator_start(const DeviceFeatureConfig *cfg);

#endif