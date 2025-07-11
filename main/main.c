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
#include "nvs_flash.h"
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
#include "storage_management.h"
#include "main_button_controller.h"

#include "log_types.h"
#include "log_writer.h"
#include "log_utils.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "notification_info_message_creator.h"
#include "general_manager.h"

static const char *TAG = "Main";

// to test use set_brightness
RegionStatusChangedInfo test_region_status_infos[][3] = {
    { { .region_id = 1, .brightness = 100 }},
    { { .region_id = 1, .brightness = 100 }, { .region_id = 2, .brightness = 100 } },
    { { .region_id = 3, .brightness = 150 }},
    { { .region_id = 1, .brightness = 200 }, { .region_id = 2, .brightness = 200 }, { .region_id = 3, .brightness = 200 } },
    { { .region_id = 1, .brightness = 0 }, { .region_id = 2, .brightness = 0 }, { .region_id = 3, .brightness = 0 } }
};


void app_main() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Button is pressed.");
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & BUTTON_PIN_BITMASK) {
            ESP_LOGI(TAG, "Awakes by GPIO!");
        
            init_transaction_manager();
            init_general_manager();

            //I2C
            init_i2c_master();
            
            //LP
            
            bool lp_exists = false;
            if(lp_exists)
            {
                init_lp_i2c_master();
                initialize_lp_core();
                initialize_lp_core_queue();
                xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 2048, NULL, 1, NULL);
            }
            init_therapy_counter_partition();

            init_log_writer();

            
            uint16_t therapy_count = read_therapy_count();
            ESP_LOGI(TAG, "Therapy count: %u", therapy_count);
            /*
            read_and_set_records(0);
            read_and_set_records(1);
            read_and_set_records(2);
            read_and_set_records(3);
            ESP_LOGI(TAG, "fragment count: %u", get_fragment_count());
            */
            /*
            //Laser Driver
            initialize_laser_drivers();
            
            //Temperature Sensor
            
            initialize_temperature_sensor();
            initialize_alert_gpios();
            xTaskCreate(temperature_read_task, "Temperature Update Task", 2048, NULL, 1, NULL);
            xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);
            
            //Proximity Sensor
            
            initialize_proximity_int_gpio();
            bool hp_prox_sensor_exists = false;
            initialize_proximity_sensors(hp_prox_sensor_exists, lp_exists);
            xTaskCreate(monitor_proximity_int_task, "Monitor Proximity Int Task", 2048, NULL, 1, NULL);
            
            //Boot Button
            */
            
            initialize_boot_button_gpio();
            xTaskCreate(monitor_boot_button_task, "Monitor Boot Botton Task", 2048, NULL, 1, NULL);
        
            ESP_LOGI(TAG, "System Ready.");

            gpio_config_t io_conf = {
                .pin_bit_mask = BUTTON_PIN_BITMASK,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config(&io_conf);
        
            // Buton kontrol task'ı
            xTaskCreate(wait_for_button_to_sleep, "button_task", 2048, NULL, 1, NULL);
            add_and_send_notification_info(DEVICE_AWAKED);
            ESP_LOGI(TAG, "Device awakes.");
            start_device();
        }
    } else {
        ESP_LOGI(TAG, "Deep sleep.");
        enter_deep_sleep();
    }

    ESP_LOGI(TAG, "System Ready.");
}
