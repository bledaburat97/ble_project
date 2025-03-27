
#include "i2c_control.h"
#include "laser_driver_control.h"
#include "timer_management.h"
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
#include "constants.h"
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
#include "therapy_controller.h"
#include "driver/rtc_io.h" 

#define BUTTON_PIN_BITMASK (1ULL << GPIO_NUM_0)  // Sadece GPIO0

static const char *TAG = "Main";

// to test use set_brightness
RegionStatusChangedInfo test_region_status_infos[][3] = {
    { { .region_id = 1, .brightness = 100 }},
    { { .region_id = 1, .brightness = 100 }, { .region_id = 2, .brightness = 100 } },
    { { .region_id = 3, .brightness = 150 }},
    { { .region_id = 1, .brightness = 200 }, { .region_id = 2, .brightness = 200 }, { .region_id = 3, .brightness = 200 } },
    { { .region_id = 1, .brightness = 0 }, { .region_id = 2, .brightness = 0 }, { .region_id = 3, .brightness = 0 } }
};

void stop_lasers() {
    stop_laser_drivers();
}

void enter_deep_sleep() {
    ESP_LOGI(TAG, "Derin uykuya geçiliyor...");
    
    // GPIO'yu INPUT + PULLUP yap (buton LOW'da tetiklesin)
    gpio_config_t io_conf = {
        .pin_bit_mask = BUTTON_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Buton LOW olduğunda uyansın (ext1 wakeup)
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);

    esp_deep_sleep_start();  // Sonsuz deep sleep
}

void wait_until_button_released() {
    gpio_config_t io_conf = {
        .pin_bit_mask = BUTTON_PIN_BITMASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);


    while (gpio_get_level(GPIO_NUM_0) == 0) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Still low");
    }
    ESP_LOGI(TAG, "Buton bırakıldı (HIGH).");
}

void app_main() {

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Butona basılarak uyanıldı.");
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & BUTTON_PIN_BITMASK) {
            ESP_LOGI(TAG, "GPIO%d ile uyandı!", GPIO_NUM_0);

            ESP_LOGI(TAG, "Kod burada.");
            esp_err_t ret;
        
            ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ret = nvs_flash_init();
            }
            ESP_ERROR_CHECK(ret);
        
            ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
        
            esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            ret = esp_bt_controller_init(&bt_cfg);
            ESP_ERROR_CHECK(ret);
        
            ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
            ESP_ERROR_CHECK(ret);
        
            ret = esp_bluedroid_init();
            ESP_ERROR_CHECK(ret);
        
            ret = esp_bluedroid_enable();
            ESP_ERROR_CHECK(ret);
        
            init_ble();
            init_device_param_status();
        

            initialize_boot_button_gpio();
            start_inactivity_timer();
            xTaskCreate(monitor_boot_button_task, "Monitor Boot Botton Task", 2048, NULL, 1, NULL);
        
            ESP_LOGI(TAG, "System Ready.");

            wait_until_button_released();
            enter_deep_sleep();
        }
    } else {
        ESP_LOGI(TAG, "İlk açılış veya farklı sebep, deep sleep'e geçilecek.");
        enter_deep_sleep();
    }

    ESP_LOGI(TAG, "Kod burada.");
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_init();
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    init_ble();
    init_device_param_status();

    //I2C
    //init_i2c_master();
    
    //LP
    /*
    bool lp_exists = true;
    if(lp_exists)
    {
        init_lp_i2c_master();
        initialize_lp_core();
        initialize_lp_core_queue();
        xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 2048, NULL, 1, NULL);
    }
    */
    //Laser Driver
    //initialize_laser_drivers();
    
    //Temperature Sensor
    /*
    initialize_temperature_sensor();
    initialize_alert_gpios();
    xTaskCreate(temperature_update_task, "Temperature Update Task", 2048, NULL, 1, NULL);
    xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);
    */
    //Proximity Sensor
    /*
    initialize_proximity_int_gpio();
    bool hp_prox_sensor_exists = false;
    initialize_proximity_sensors(hp_prox_sensor_exists, lp_exists);
    xTaskCreate(monitor_proximity_int_task, "Monitor Proximity Int Task", 2048, NULL, 1, NULL);
    */
    //Boot Button
    initialize_boot_button_gpio();
    start_inactivity_timer();
    xTaskCreate(monitor_boot_button_task, "Monitor Boot Botton Task", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "System Ready.");
}
