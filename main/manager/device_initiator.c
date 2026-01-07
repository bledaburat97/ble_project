#include "device_initiator.h"

#include "device_event_router.h"
#include "state_controller.h"
#include "timer_controller.h"

#include "../button/main_button_controller.h"

#include "../i2c/temperature/temperature_sensor_controller.h"
#include "../i2c/proximity/proximity_int_controller.h"
#include "../i2c/proximity/proximity_sensor_controller.h"
#include "../i2c/laser/laser_driver_controller.h"

#include "../ble/include/ble_internal.h"
#include "../storage/log_writer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "DeviceInitiator";

static const bool is_uncompleted_therapy_recoverable = false;
static const bool is_hp_prox_sensor_active = true;
static const bool is_lp_prox_sensor_active = true;

void init_device_manager(void)
{
    init_device_event_router();

    init_state_controller();
    init_timer_controller();

    if (is_uncompleted_therapy_recoverable) {
        UncompletedTherapyInfo info;
        if (read_uncompleted_therapy(&info)) {
            ESP_LOGI(TAG, "Uncompleted therapy found in flash.");
            post_uncompleted_therapy_set_event(info);
        } else {
            post_device_start_event();
        }
    } else {
        post_device_start_event();
    }

    register_timer_end_callback(post_timer_completed_event);
    register_temp_alert_callback(post_temperature_alert_event);

    register_on_write_activation_callback(post_activation_request_event);
    register_on_write_updating_therapy_state_callback(post_therapy_state_change_request_event);

    register_button_press_callback(post_button_press_event);

    initialize_proximity_int_gpio();
    xTaskCreate(monitor_proximity_int_task, "monitor_proximity_int", 4096, NULL, 1, NULL);

    initialize_proximity_sensors(is_hp_prox_sensor_active, is_lp_prox_sensor_active, post_helmet_state_change_event);

    ESP_LOGI(TAG, "Device initialized");
}
