#include "device_initiator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// ===== Storage =====
#include "storage/therapy_counter.h"
#include "storage/log_partition_manager.h"
#include "storage/log_orchestrator.h"
#include "storage/log_resume.h"
#include "storage/log_types.h"
#include "storage/profile_partition_manager.h"

// ===== BLE / Transaction =====
#include "ble/include/ble_controller.h"
#include "transaction/message_queue_manager.h"
#include "transaction/passkey_handler.h"
#include "transaction/default_configuration_handler.h"
#include "transaction/notification_info_message_creator.h"
#include "transaction/device_info_message_creator.h"
#include "transaction/measurement_info_message_creator.h"
#include "transaction/timer_state_info_message_creator.h"
#include "transaction/records_info_message_creator.h"
#include "transaction/wifi_config_handler.h"
#include "ble/include/ble_internal.h"

// ===== I2C + LP core =====
#include "i2c/i2c_control.h"
#include "lp_core/lp_core_main.h"
#include "lp_core/lp_core_queue_manager.h"

// ===== Proximity =====
#include "i2c/proximity/proximity_int_controller.h"
#include "i2c/proximity/proximity_sensor_controller.h"

// ===== Laser drivers =====
#include "i2c/laser/laser_driver_controller.h"

// ===== Sensors =====
#include "humidity/humidity_sensor_controller.h"
#include "temperature/temp_sensor_manager.h"

// ===== Button =====
#include "button/main_button_controller.h"

// ===== State / Manager =====
#include "device_event_router.h"
#include "state_controller.h"
#include "timer_controller.h"

// ===== Indicators / misc =====
#include "state/mode_selector.h"
#include "state/deep_sleep_manager.h"

// ===== Messaging / logs =====
#include "manager/message_saver.h"

static const char *TAG = "DeviceInitiator";

// ---------- helpers ----------
static void init_message_creators(void)
{
    init_notification_info_message_creator();
    init_device_info_message_creator();
    init_measurement_info_message_creator();
    init_timer_state_info_message_creator();
    init_records_info_message_creator();
}

static void init_storage(const DeviceFeatureConfig *cfg)
{
    if (cfg->therapy_counter_partition_active) {
        init_therapy_counter_partition();
        uint16_t therapy_count = read_therapy_count();
        ESP_LOGI(TAG, "Therapy count: %u", therapy_count);
    }

    if (cfg->log_partition_active) {
        init_log_partition();
    }

    if (cfg->log_orchestrator_active) {
        log_orchestrator_init();
    }
}

static void init_i2c_and_lp_core(const DeviceFeatureConfig *cfg)
{
    // HP I2C master her durumda (laser + bazı sensorlar) kullanılabilir.
    init_i2c_master();

    if (cfg->lp_prox_sensor_active) {
        init_lp_i2c_master();
        initialize_lp_core();
        initialize_lp_core_queue();
        xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 4096, NULL, 5, NULL);
    }
}

static void init_laser_drivers_and_default_brightness(const DeviceFeatureConfig *cfg)
{
    if (!cfg->laser_and_led_drivers_active) return;

    initialize_laser_drivers();

    if (cfg->apply_default_brightness_on_boot) {
        const uint8_t *brightness_list = get_default_brightness();

        // passed_seconds=0 varsayımı (startup)
        change_brightness(brightness_list);
        save_log(NOTIF_BRIGHTNESS_UPDATED, brightness_list, 6, 0);

        send_notification_info(NOTIF_BRIGHTNESS_UPDATED, 0);
    }
}

static void init_ble_and_transaction(const DeviceFeatureConfig *cfg)
{
    if (!cfg->ble_active) return;

    init_ble();
    init_message_queue_manager();
    init_profile_partition();
    init_passkey_handler();
    init_default_configuration_handler();
    init_message_creators();

    // Eğer Wi-Fi config BLE üzerinden yönetiliyorsa:
    if (cfg->wifi_config_active) {
        init_wifi_config();
    }
}

static void init_buttons(const DeviceFeatureConfig *cfg)
{
    if (!cfg->deep_sleep_button_control_active) return;

    set_deep_sleep_button();
    xTaskCreate(wait_for_button_to_sleep, "button_task", 4096, NULL, 5, NULL);
}

static void init_sensors_and_tasks(const DeviceFeatureConfig *cfg)
{
    // Proximity INT + task
    initialize_proximity_int_gpio();
    xTaskCreate(monitor_proximity_int_task, "monitor_proximity_int", 4096, NULL, 1, NULL);

    // Proximity sensors init
    initialize_proximity_sensors(
        cfg->hp_prox_sensor_active,
        cfg->lp_prox_sensor_active,
        post_helmet_state_change_event
    );

    // Humidity
    initialize_humidity_sensor();
    xTaskCreate(humidity_read_task, "humidity_read_task", 2048, NULL, 1, NULL);

    // Temperature manager + tasks
    temp_sensor_manager_initialize();
    xTaskCreate(temp_sensor_manager_temperature_read_task, "Temperature Read Task", 2048, NULL, 1, NULL);
    xTaskCreate(temp_sensor_manager_alert_monitor_task, "Temperature Alert Task", 2048, NULL, 1, NULL);
}

static void init_state_machine(const DeviceFeatureConfig *cfg)
{
    // Event router + controllers
    init_device_event_router();
    init_state_controller();
    init_timer_controller();

    // Callbacks -> events
    register_timer_end_callback(post_timer_completed_event);
    temp_sensor_manager_register_temp_alert(post_temperature_alert_event);

    if (cfg->ble_active) {
        register_on_write_activation_callback(post_activation_request_event);
        register_on_write_updating_therapy_state_callback(post_therapy_state_change_request_event);
        register_on_change_in_profile_id_during_active(post_stop_request_for_profile_change);
    }

    register_button_press_callback(post_button_press_event);

    // Mode indicator (opsiyonel)
    if (cfg->mode_indicator_active) {
        initialize_mode_indicator_gpio();
    }

    // Startup / restore event:
    if (cfg->continue_uncompleted_therapy) {
        UncompletedTherapyInfo info;
        if (log_resume_read_uncompleted_therapy(&info)) {
            ESP_LOGI(TAG, "Uncompleted therapy found in flash.");
            post_uncompleted_therapy_set_event(info);
        } else {
            post_device_start_event();
        }
    } else {
        post_device_start_event();
    }
}

static void finalize_startup(const DeviceFeatureConfig *cfg)
{
    if (cfg->creating_logs_permitted) {
        add_and_send_notification_info(DEVICE_AWAKED);
    }

    ESP_LOGI(TAG, "Device initialized. System Ready.");
}

void device_initiator_start(const DeviceFeatureConfig *cfg)
{
    if (!cfg) return;

    ESP_LOGI(TAG, "Device initiator start.");

    // 1) Storage
    init_storage(cfg);

    // 2) I2C + LP core (LP queue task burada)
    init_i2c_and_lp_core(cfg);

    // 3) Laser drivers + default brightness
    init_laser_drivers_and_default_brightness(cfg);

    // 4) BLE + transaction
    init_ble_and_transaction(cfg);

    // 5) Sensors + tasks (proximity, humidity, temp)
    init_sensors_and_tasks(cfg);

    // 6) Buttons (sleep button task)
    init_buttons(cfg);

    // 7) State machine wiring + DeviceStart/Restore event (en sona yakın)
    if (cfg->state_and_timer_active) {
        init_state_machine(cfg);
    } else {
        ESP_LOGW(TAG, "state_and_timer_active is false; state machine not started.");
    }

    // 8) Finalize
    finalize_startup(cfg);
}
