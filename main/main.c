#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "transaction/default_configuration_handler.h"
#include "transaction/matching_message_encoder.h"
#include "transaction/notification_info_message_creator.h"
#include "transaction/passkey_handler.h"
#include "transaction/records_info_message_creator.h"
#include "transaction/measurement_info_message_creator.h"
#include "transaction/timer_state_info_message_creator.h"
#include "transaction/message_queue_manager.h"
#include "transaction/incoming_message_handler.h"
#include "transaction/device_info_message_creator.h"
#include "transaction/wifi_config_handler.h"

#include "state/deep_sleep_manager.h"
#include "state/general_manager.h"
#include "state/timer_manager.h"
#include "state/state_manager.h"
#include "state/mode_selector.h"
#include "state/current_therapy_info_manager.h"

#include "i2c/i2c_control.h"
#include "i2c/laser/laser_driver_controller.h"
#include "i2c/proximity/proximity_int_controller.h"
#include "i2c/proximity/proximity_sensor_config.h"
#include "i2c/proximity/proximity_sensor_controller.h"

#include "i2c/temperature/temperature_alert_controller.h"
#include "i2c/temperature/temperature_sensor_controller.h"
#include "i2c/humidity/humidity_sensor_controller.h"

#include "button/main_button_controller.h"

#include "storage/log_partition_manager.h"
#include "storage/log_types.h"
#include "storage/log_utils.h"
#include "storage/log_writer.h"
#include "storage/therapy_counter.h"

#include "nvs/storage_manager.h"

#include "ble/include/ble_controller.h"

#include "lp_core/lp_core_main.h"
#include "lp_core/lp_core_queue_manager.h"

#include "buzzer/buzzer.h"

#include "lp_core_firmware.h"
#include "nvs_flash.h"

static const char *TAG = "Main";

void periodic_message_sender_task(void *pvParameters) {
    const TickType_t delay_ticks = pdMS_TO_TICKS(30 * 1000); 
    while (1) {
        ESP_LOGI(TAG, "Sending records info message for therapy ID 1...");
        send_records_info_message(1);

        vTaskDelay(delay_ticks);
    }
}

typedef struct {
    bool ble_active;
    bool state_and_timer_active;
    bool lp_prox_sensor_active;
    bool therapy_counter_partition_active;
    bool log_partition_active;
    bool laser_and_led_drivers_active;
    bool temperature_sensor_active;
    bool hp_prox_sensor_active;
    bool default_sleep_active;
    bool deep_sleep_button_control_active;
    bool creating_logs_permitted;
    bool continue_uncompleted_therapy;
} feature_config_t;

static const feature_config_t feature_config = {
    .ble_active = true,
    .state_and_timer_active = true,
    .lp_prox_sensor_active = true,
    .therapy_counter_partition_active = true,
    .log_partition_active = true,
    .laser_and_led_drivers_active = true,
    .temperature_sensor_active = true,
    .hp_prox_sensor_active = true,
    .default_sleep_active = false,
    .deep_sleep_button_control_active = true,
    .creating_logs_permitted = true,
    .continue_uncompleted_therapy = false,
};

static void initialize_nvs_flash_module(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
 
static bool activate_device_if_needed(const feature_config_t *config) {
    if (!config->default_sleep_active) {
        return true;
    }
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Button is pressed.");
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & BUTTON_PIN_BITMASK) {
            ESP_LOGI(TAG, "Awakes by GPIO!");
            return true;
        }
        ESP_LOGW(TAG, "Wake-up did not originate from the expected GPIO pin.");
        return false;
    }
    ESP_LOGI(TAG, "Deep sleep.");
    add_notification_log(NOTIF_DEVICE_NOT_AWAKED, 0);
    enter_deep_sleep();
    return false;
}

static void initialize_storage_components(const feature_config_t *config) {
    init_nvs();

    if (config->therapy_counter_partition_active) {
        init_therapy_counter_partition();
        uint16_t therapy_count = read_therapy_count();
        ESP_LOGI(TAG, "Therapy count: %u", therapy_count);
    }
    if (config->log_partition_active) {
        init_log_partition();
    }
}

static void init_message_creators(){
    init_notification_info_message_creator();
    init_device_info_message_creator();
    init_measurement_info_message_creator();
    init_timer_state_info_message_creator();
    init_records_info_message_creator();
}

static void initialize_ble_components(const feature_config_t *config) {
    if (config->ble_active) {
        init_ble();
        init_message_queue_manager();
        init_incoming_message_handler();
        init_passkey_handler();
        init_default_configuration_handler();
        init_message_creators();
    }

}

static void initialize_state_management_components(const feature_config_t *config) {
    if (config->state_and_timer_active) {
        init_timer_manager();
        init_general_manager();
    }
}

static void initialize_lp_core_components(const feature_config_t *config) {
    if (!config->lp_prox_sensor_active) {
        return;
    }

    init_lp_i2c_master();
    initialize_lp_core();
    initialize_lp_core_queue();
    xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 4096, NULL, 5, NULL);
}

static void initialize_temperature_components(const feature_config_t *config) {
    if (!config->temperature_sensor_active) {
        return;
    }
    initialize_temperature_sensor();
    initialize_humidity_sensor();
    initialize_alert_gpios();
    xTaskCreate(temperature_read_task, "Temperature Update Task", 2048, NULL, 1, NULL);
    xTaskCreate(humidity_read_task, "humidity_read_task", 2048, NULL, 1, NULL);

    //xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);
}

static void initialize_proximity_components(const feature_config_t *config) {
    if (!config->lp_prox_sensor_active && !config->hp_prox_sensor_active) {
        return;
    }
    initialize_proximity_int_gpio();
    initialize_proximity_sensors(config->hp_prox_sensor_active, config->lp_prox_sensor_active);
    xTaskCreate(monitor_proximity_int_task, "Monitor Proximity Int Task", 4096, NULL, 1, NULL);
    //xTaskCreate(proximity_read_task, "ProximityReadTask", 2048, NULL, 5, NULL);
}

static void initialize_laser_components(const feature_config_t *config) {
    if (!config->laser_and_led_drivers_active) {
        return;
    }
    initialize_laser_drivers();
    const uint8_t *brightness_list = get_default_brightness();

    for (int i = 0; i < TOTAL_REGION_COUNT; i++) {
        set_brightness_of_region(i + 1, brightness_list[i]);
    }

    //set_brightness_of_region(1, 20);
    //set_brightness_of_region(2, 20);
    //set_brightness_of_region(3, 20);
    //set_brightness_of_region(4, 20);
}

static void initialize_sensor_and_driver_components(const feature_config_t *config) {
    init_i2c_master();
    initialize_lp_core_components(config);
    initialize_laser_components(config);
    initialize_temperature_components(config);
    initialize_proximity_components(config);
}

static void initialize_button_components(const feature_config_t *config) {
    if (config->deep_sleep_button_control_active) {
        set_deep_sleep_button();
        xTaskCreate(wait_for_button_to_sleep, "button_task", 4096, NULL, 5, NULL);
    }
}

static void finalize_device_startup(const feature_config_t *config) {

    if(config->continue_uncompleted_therapy) {
        bool restored = restore_uncompleted_therapy_if_exists();
        if (restored) {
            ESP_LOGI(TAG, "Device booted with an uncompleted therapy. Current therapy set to PAUSED.");
        }
        set_continue_uncompleted_therapy(true);
    }

    if (config->creating_logs_permitted) {
        add_and_send_notification_info(DEVICE_AWAKED);
    }

    ESP_LOGI(TAG, "Device awakes.");

    if (config->state_and_timer_active) {
        start_device();
    }

    ESP_LOGI(TAG, "System Ready.");
}



void app_main(void) {
    initialize_nvs_flash_module();

    if (!activate_device_if_needed(&feature_config)) {
        return;
    }

    initialize_storage_components(&feature_config);
    initialize_ble_components(&feature_config);
    initialize_state_management_components(&feature_config);

    //xTaskCreate(periodic_message_sender_task, "PeriodicMsgSender", 2048, NULL, 5, NULL);

    initialize_sensor_and_driver_components(&feature_config);
    initialize_button_components(&feature_config);
    finalize_device_startup(&feature_config);
    initialize_mode_indicator_gpio();
    init_wifi_config();
}