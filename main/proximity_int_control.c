#include "temperature_alarm_control.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "proximity_sensor_control.h"
#include "state_manager.h"
#include "device_configuration.h"

static const char *TAG = "ProximityInt";
const int prox_sensor_int_gpio_list[MAX_NUM_OF_SENSORS] = {FIRST_PROX_SENSOR_INT_GPIO, SECOND_PROX_SENSOR_INT_GPIO};

static void checkProximitySensor(uint8_t asserted_sensor_index){
    /*
    if(get_device_state() == STATE_ACTIVE) {
        //TODO: STOP LASERS
    }
        */
    ESP_LOGI(TAG, "Check Proximity Sensor");

    request_excess_status(asserted_sensor_index == 1);
}

void monitor_proximity_int_task(void *param) {
    while (1) {
        for (int i = 1; i < MAX_NUM_OF_SENSORS; i++) { //TODO: i= 0'dan başlatmak gerekiyor mu, niye böyle?
            int current_level = gpio_get_level(prox_sensor_int_gpio_list[i]);

            if (current_level != NORMAL_PIN_STATUS) {
                ESP_LOGI(TAG, "Proximity Sensor %d: INT is triggered!", i);
                checkProximitySensor(i);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void initialize_proximity_int_gpio() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT; 
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;


    for (int i = 0; i < MAX_NUM_OF_SENSORS; i++) {
        io_conf.pin_bit_mask = (1ULL << prox_sensor_int_gpio_list[i]);
        gpio_config(&io_conf);
        ESP_LOGI(TAG, "Proximity sensor %d INT pin monitoring started on GPIO %d", i, prox_sensor_int_gpio_list[i]);
    }

}
