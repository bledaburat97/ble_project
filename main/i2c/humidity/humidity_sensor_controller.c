#include "humidity_sensor_controller.h"

#include "humidity_sensor.h"
#include "../i2c_control.h"
#include "../../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "HumiditySensorController";

static const uint8_t humidity_sensor_address = HDC1080_I2C_ADDRESS;

static uint8_t last_notified_humidity = 0xFF;

static void (*humidity_update_callback)(uint8_t) = NULL;

void register_humidity_update(void (*callback)(uint8_t))
{
    humidity_update_callback = callback;
}

// 0-100% RH aralığını 1 byte'a çevir (0-100)
static uint8_t convert_rh_to_byte(float rh)
{
    if (rh < 0.0f)  rh = 0.0f;
    if (rh > 100.0f) rh = 100.0f;

    return (uint8_t)lroundf(rh); // 0..100
}

static uint8_t to_bucket_5(uint8_t humidity_byte)
{
    return humidity_byte - (humidity_byte % 5);
}

static float round_to_one_percent(float rh)
{
    return roundf(rh); // 1% çözünürlük
}

static float read_value() {
    float temp, rh;
    hdc1080_trigger_measurement_trh(humidity_sensor_address);
    vTaskDelay(pdMS_TO_TICKS(20));
    hdc1080_read_temperature_humidity(humidity_sensor_address, &temp, &rh);
    return round_to_one_percent(rh);
}

uint8_t measure_and_get_humidity()
{
    float humidity = read_value();
    uint8_t humidity_byte = convert_rh_to_byte(humidity);
    last_notified_humidity = to_bucket_5(humidity_byte);
    return last_notified_humidity;
}

uint8_t get_humidity()
{
    if (last_notified_humidity == 0xFF) {
        return measure_and_get_humidity();
    }
    return last_notified_humidity;
}

void initialize_humidity_sensor()
{
    ESP_LOGI(TAG, "Initialize HDC1080 humidity sensor.");

    // Default konfig (datasheet reset value 0x1000) MODE=0, HRES=14bit, TRES=14bit 
    HDC1080_ConfigReg cfg = {0};
    cfg.rst   = 0;                               // normal
    cfg.heat  = 0;                               // heater kapalı
    cfg.mode  = HDC1080_MODE_BOTH;              // T + RH arka arkaya
    cfg.btst  = 0;                               // yazma
    cfg.tres  = HDC1080_TRES_14BIT;             // maks doğruluk
    cfg.hres  = HDC1080_HRES_11BIT;             // hız/enerji için 11 bit (istersen 14'e çek)
    cfg.reserved_low  = 0;
    cfg.reserved_high = 0;

    hdc1080_set_configuration(humidity_sensor_address, cfg);

    float humidity = read_value();
    ESP_LOGI(TAG, "Initial humidity: %.2f%% (bucket=%u%%)", humidity, last_notified_humidity);
}

void humidity_read_task(void *param)
{
    while (1) {
        float humidity = read_value();
        uint8_t humidity_byte = convert_rh_to_byte(humidity);

        if (fabsf(last_notified_humidity - humidity_byte) >= 5) {
            if (humidity_update_callback) {
                last_notified_humidity = to_bucket_5(humidity_byte);
                humidity_update_callback(last_notified_humidity);
            } else {
                ESP_LOGW(TAG, "Humidity update callback is not registered.");
            }
        }

        //ESP_LOGI(TAG, "Humidity measured: %.2f%%", humidity);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
