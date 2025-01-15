#include "i2c_control.h"
#include "laser_driver_control.h"
#include "ble_control.h"
#include "proximity_sensor_control.h"
#include "temperature_sensor_control.h"
#include "timer_management.h"
#include "therapy_controller.h"

#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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

#include "sdkconfig.h"

void app_main() {
    esp_err_t ret;

    // Initialize NVS
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
    start_inactivity_timer();

    initialize_i2c();
    initialize_laser_driver();
    initialize_proximity_sensors();
    initialize_temperature_sensor();
    
    //TODO: kullanılmayacak ama storagedan bir şey okuma örneği.
    float last_temperature = read_temperature_from_nvs();
    ESP_LOGI("App", "Last temperature from NVS: %.2f°C", last_temperature);

    // Start temperature update task
    xTaskCreate(temperature_update_task, "Temperature Update Task", 4096, NULL, 5, NULL);
}

//Call when esp32 detects INT pin of any proximity sensor HIGH.

void checkProximitySensor(uint8_t asserted_sensor_index){
    if(get_helmet_status()) {
        //TODO: STOP LASERS
    }
    if(get_sensor_detection_status(asserted_sensor_index)){
        if(check_threshold_exceeded(asserted_sensor_index, false)) {
            set_sensor_detection_status(asserted_sensor_index, false);
            set_default_thresholds(asserted_sensor_index);
            reset_interrupt(asserted_sensor_index);
            set_helmet_status(false);
            send_notification(HELMET_OFF);

        }
        else
        {
            send_notification(WRONG_HELMET_STATUS);
            reset_interrupt(asserted_sensor_index);
            //TODO: TRY TO START LASERS AGAIN
        }
    }
    else {
        if(check_threshold_exceeded(asserted_sensor_index, true)) {
            set_sensor_detection_status(asserted_sensor_index, true);
            increase_thresholds(asserted_sensor_index);
            reset_interrupt(asserted_sensor_index);
            if(check_other_sensor_detected(asserted_sensor_index)) {
                set_helmet_status(true);
            }
        }
        else {
            send_notification(HELMET_ON);
            reset_interrupt(asserted_sensor_index);
        }
    }

}

//Call when esp32 detects ALERT pin of any temperature sensor as LOW
void checkTemperatureSensor(uint8_t asserted_sensor_index) {
    //STOP LASERS
    uint8_t temp_data[7];
    temp_data[0] = asserted_sensor_index; //indicates sensor index which sends alert
    get_temperature_of_all_sensors(&temp_data[1]);
    send_aperiodic_info(get_temperature_handle(), temp_data, sizeof(temp_data));
}