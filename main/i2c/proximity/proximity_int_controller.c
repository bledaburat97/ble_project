#include "proximity_int_controller.h"

#include "proximity_sensor_controller.h"
#include "../../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "ProximityIntController";

static const int INT_GPIO_LIST[MAX_NUM_OF_SENSORS] = {
    FIRST_PROX_SENSOR_INT_GPIO,
    SECOND_PROX_SENSOR_INT_GPIO
};

static void check_proximity_sensor(uint8_t asserted_sensor_index)
{
    ESP_LOGI(TAG, "Proximity INT triggered (sensor_index=%u)", asserted_sensor_index);

    bool is_lp = (asserted_sensor_index == 1U);
    request_excess_status(is_lp);
}

void monitor_proximity_int_task(void *param)
{
    (void)param;

    while (1) {
        for (int i = 0; i < MAX_NUM_OF_SENSORS; i++) {
            int current_level = gpio_get_level(INT_GPIO_LIST[i]);

            if (current_level != NORMAL_PIN_STATUS) {
                ESP_LOGI(TAG, "Proximity sensor %d: INT is asserted (level=%d)", i, current_level);
                check_proximity_sensor((uint8_t)i);
            }
        }

        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        if (watermark < 100) {
            ESP_LOGW(TAG, "Low stack watermark: %u words", watermark);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void initialize_proximity_int_gpio(void)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    for (int i = 0; i < MAX_NUM_OF_SENSORS; i++) {
        io_conf.pin_bit_mask = (1ULL << INT_GPIO_LIST[i]);
        gpio_config(&io_conf);
        ESP_LOGI(TAG,
                 "Proximity sensor %d INT pin monitoring started on GPIO %d",
                 i, INT_GPIO_LIST[i]);
    }
}