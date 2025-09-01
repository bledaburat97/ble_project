/*#include "storage_management.h" 
#include "transaction_manager.h"
#include "i2c_control.h"
#include "lp_core_main.h"
#include "lp_core_queue_manager.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"
#include "temperature_alarm_control.h"
#include "proximity_int_control.h"
#include "proximity_sensor_control.h"
#include "deep_sleep_manager.h"
#include "boot_button_control.h"

#include "driver/rtc_io.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "proximity_sensor_config.h"


#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "esp_task_wdt.h"
#include "lp_core_firmware.h"

*/


#include "i2c_control.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"

#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "esp_task_wdt.h"
#include "temperature_alarm_control.h"
#include "lp_core_main.h"
#include "lp_core_firmware.h"
//////
#include "driver/rtc_io.h"
#include "esp_sleep.h"
//////
#include "lp_core_queue_manager.h"
#include "proximity_sensor_control.h"
#include "proximity_sensor_config.h"
#include "proximity_int_control.h"
#include "boot_button_control.h"
#include "transaction_manager.h"
#include "state_manager.h"
#include "deep_sleep_manager.h"
#include "main_button_controller.h"

#include "storage/log_types.h"
#include "storage/log_writer.h"
#include "storage/log_utils.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "notification_info_message_creator.h"
#include "general_manager.h"
#include "nvs_flash.h"
#include "storage/log_partition_manager.h"
#include "records_info_message_creator.h"

static const char *TAG = "Main";

static bool is_ble_active = true;
static bool is_state_and_timer_active = false;
static bool is_lp_prox_sensor_active = false;
static bool is_therapy_counter_partition_active = true;
static bool is_log_partition_active = true;
static bool is_laser_and_led_drivers_active = true;
static bool is_temperature_sensor_active = false;
static bool is_hp_prox_sensor_active = false;
static bool is_boot_button_control_active = false;
static bool is_default_sleep_active = false;
static bool is_deep_sleep_button_control_active = false;
static bool is_creating_logs_permitted = false;

void periodic_message_sender_task(void *pvParameters) {
    const TickType_t delay_ticks = pdMS_TO_TICKS(30 * 1000); 
    while (1) {
        ESP_LOGI(TAG, "Sending records info message for therapy ID 1...");
        send_records_info_message(1);

        vTaskDelay(delay_ticks);
    }
}

void app_main() {
   esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    bool is_device_active = false;

    if(is_default_sleep_active) {
        if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
            ESP_LOGI(TAG, "Button is pressed.");
            uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pins & BUTTON_PIN_BITMASK) {
                ESP_LOGI(TAG, "Awakes by GPIO!");
                is_device_active = true;
            }
        }
        else{
            ESP_LOGI(TAG, "Deep sleep.");
            enter_deep_sleep();
        }
    }
    else{
        is_device_active = true;
    }
    
    if(is_device_active) {

        if(is_therapy_counter_partition_active) {
            init_therapy_counter_partition();
            uint16_t therapy_count = read_therapy_count();
            ESP_LOGI(TAG, "Therapy count: %u", therapy_count);
        }

        if(is_log_partition_active) {
            init_log_partition();
        }

        if(is_ble_active) {
            init_transaction_manager();
        }

        if(is_state_and_timer_active) {
            init_general_manager();
        }

        init_i2c_master();
        //xTaskCreate(i2c_scanner_task, "i2c_scanner_task", 2048, NULL, 5, NULL);

        if(is_lp_prox_sensor_active) {
            init_lp_i2c_master();
            initialize_lp_core();
            initialize_lp_core_queue();
            xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 4096, NULL, 5, NULL);
        }

        //test_add_log_flow();
        //xTaskCreate(periodic_message_sender_task, "PeriodicMsgSender", 2048, NULL, 5, NULL);

        /*
            read_and_set_records(0);
            read_and_set_records(1);
            read_and_set_records(2);
            read_and_set_records(3);
            ESP_LOGI(TAG, "fragment count: %u", get_fragment_count());
        */

        if(is_laser_and_led_drivers_active) {
            initialize_laser_drivers();
            set_laser_drivers_status(true);
            //set_brightness_of_region(1, 20);
            set_brightness_of_region(2, 20);
            set_brightness_of_region(3, 20);
            //set_brightness_of_region(4, 20);
        }
                
        if(is_temperature_sensor_active) {
            initialize_temperature_sensor();
            initialize_alert_gpios();
            xTaskCreate(temperature_read_task, "Temperature Update Task", 2048, NULL, 1, NULL);
            xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);
        }
        
        if(is_lp_prox_sensor_active || is_hp_prox_sensor_active) {
            initialize_proximity_int_gpio();
            initialize_proximity_sensors(is_hp_prox_sensor_active, is_lp_prox_sensor_active);
            xTaskCreate(monitor_proximity_int_task, "Monitor Proximity Int Task", 2048, NULL, 1, NULL);
            xTaskCreate(proximity_read_task, "ProximityReadTask", 2048, NULL, 5, NULL);
        }
                        
        if(is_boot_button_control_active) {
            initialize_boot_button_gpio();
            xTaskCreate(monitor_boot_button_task, "Monitor Boot Botton Task", 2048, NULL, 1, NULL);
        }

        if (is_deep_sleep_button_control_active) {
            set_deep_sleep_button();
            xTaskCreate(wait_for_button_to_sleep, "button_task", 2048, NULL, 1, NULL);
        }

        if(is_creating_logs_permitted) {
            add_and_send_notification_info(DEVICE_AWAKED);
        }

        ESP_LOGI(TAG, "Device awakes.");

        if(is_state_and_timer_active) {
            start_device();
        }

        ESP_LOGI(TAG, "System Ready.");
    
    }

}
