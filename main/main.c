
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

#define MAX_BRIGHTNESS 0xFF
#define DEBOUNCE_TIME_MS 200
#define NUM_OF_CONFIGS 5
#define NUM_OF_REGIONS 3
#define NUM_OF_LASER_DRIVERS 3

static const char *TAG = "Main";
static QueueHandle_t button_queue;
static int current_index = 0;
static bool laser_test = false;
static bool timer_test = false;
static bool temperature_test = true;

// to test
RegionStatusChangedInfo region_status_infos[][3] = {
    { { .region_id = 1, .brightness = 100 }},
    { { .region_id = 1, .brightness = 100 }, { .region_id = 2, .brightness = 100 } },
    { { .region_id = 3, .brightness = 150 }},
    { { .region_id = 1, .brightness = 200 }, { .region_id = 2, .brightness = 200 }, { .region_id = 3, .brightness = 200 } },
    { { .region_id = 1, .brightness = 0 }, { .region_id = 2, .brightness = 0 }, { .region_id = 3, .brightness = 0 } }
};

static void IRAM_ATTR button_isr_handler(void *arg) {
    int button_pressed = 1;
    xQueueSendFromISR(button_queue, &button_pressed, NULL);
}

static void set_laser_drivers_status(bool status) {
    for(int laser_driver_index = 0; laser_driver_index < NUM_OF_LASER_DRIVERS; laser_driver_index++) {
        gpio_set_level(laser_driver_gpios[laser_driver_index], status);
    }
}

// Buton durumu kontrol eden task
void button_task(void *arg) {
    int button_pressed;
    while (1) {
        if (xQueueReceive(button_queue, &button_pressed, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Button Pressed!");
    
            //--- FOR TEST
            if(laser_test)
            {
                if(current_index == 0) {
                    set_laser_drivers_status(true);
                }

                set_brightness(region_status_infos[current_index], NUM_OF_REGIONS);
                current_index = (current_index + 1) % NUM_OF_CONFIGS;
                vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            }
            else if(timer_test)
            {
                if(current_index == 0) {
                    ESP_LOGI(TAG, "Start therapy timer!");

                    uint8_t last_therapy_data[10];
                    get_last_therapy_data(last_therapy_data);

                    start_therapy_timer(20);
                }
                else if(current_index == 1) {
                    ESP_LOGI(TAG, "Stop therapy timer!");
                    stop_therapy_timer();
                }

                current_index = (current_index + 1) % 2;

            }

            else if(temperature_test)
            {
                log_temperature();
            }
        }
    }
}

void stop_lasers() {
    stop_laser_drivers();
}

void check_lp_core_status() {
    if (ulp_lp_core_value == 0) {
        ESP_LOGE("HP_CORE", "LP-Core işlemi başarisiz!");
    } else {
        ESP_LOGI("HP_CORE", "lp_core_value: %lu", ulp_lp_core_value);
    }
}

void app_main() {
    esp_err_t ret;
    
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialize_i2c();
    //lp_core_init();


    if(laser_test)
    {
        initialize_laser_drivers();
    }
    else if(timer_test)
    {
        ESP_LOGI(TAG, "Start inactivity timer!");
        start_inactivity_timer();
    }
    else if(temperature_test) {
        ESP_LOGI(TAG, "Start temperature measurements");
        initialize_temperature_sensor();
    }
    initialize_alert_gpio();

    //--- FOR TEST
    // LEDC (PWM) yapılandırması
    
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,   // ESP32-C6 için LOW_SPEED_MODE kullan
        .duty_resolution = LEDC_TIMER_8_BIT, // 8-bit çözünürlük
        .timer_num = LEDC_TIMER_0,           // Timer 0 kullan
        .freq_hz = 5000,                     // 5 kHz frekans
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,           // Kanal 0 kullan
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = RGB_LED_GPIO,            // ESP32-C6 Mini 1'in dahili RGB LED pini GPIO8
        .duty = 0,                           // Başlangıçta LED kapalı
        .hpoint = 0,
        .flags.output_invert = 1
    };
    ledc_channel_config(&ledc_channel);
    //---
    
    gpio_config_t io_conf_laser_driver = {
        .pin_bit_mask = (1ULL << laser_driver_gpios[0]) | (1ULL << laser_driver_gpios[1]) | (1ULL << laser_driver_gpios[2]),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_laser_driver);

    gpio_config_t io_conf_button = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf_button);
    ledc_fade_func_install(0);

    set_laser_drivers_status(false);

    button_queue = xQueueCreate(10, sizeof(int));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return;
    }
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);

    xTaskCreate(button_task, "button_task", 2048, NULL, 1, NULL);
    xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);

    //xTaskCreate(temperature_update_task, "Temperature Update Task", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "System Ready.");
    
    while (1) {
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_command: %lu", ulp_lp_core_command);
        log_temperature();
        check_lp_core_status();
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_command2: %lu", ulp_lp_core_command);
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_register: %lu", ulp_lp_core_register);
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_value: %lu", ulp_lp_core_value);
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_device_address: %lu", ulp_lp_core_device_address);
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_byte_count: %lu", ulp_lp_core_byte_count);
        ESP_LOGI(TAG, "LP-Core ulp_lp_core_result: %lu", ulp_lp_core_result);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
