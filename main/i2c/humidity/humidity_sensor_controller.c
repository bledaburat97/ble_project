#include "humidity_sensor_controller.h"

#include "humidity_sensor.h"
#include "../i2c_control.h"
#include "../../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "HumiditySensorController";

static uint8_t s_last_notified_humidity = 0xFF;

static void (*s_humidity_update_callback)(uint8_t) = NULL;

/**
 * @brief Register a callback for humidity updates.
 *
 * @param callback 0–100 aralığında, 5'lik bucket'a yuvarlanmış nem
 *                 değeri alan callback. NULL gönderilirse callback temizlenir.
 */
void register_humidity_update(void (*callback)(uint8_t))
{
    s_humidity_update_callback = callback;
}

/**
 * @brief Convert relative humidity (%) to 0–100 uint8 representation.
 */
static uint8_t convert_rh_to_byte(float rh)
{
    if (rh < 0.0f) {
        rh = 0.0f;
    }
    if (rh > 100.0f) {
        rh = 100.0f;
    }

    return (uint8_t)lroundf(rh); // 0..100
}

/**
 * @brief Map humidity byte to 5% bucket (0,5,10,...,100).
 */
static uint8_t to_bucket_5(uint8_t humidity_byte)
{
    return (uint8_t)(humidity_byte - (humidity_byte % 5));
}

/**
 * @brief Round humidity to nearest 1% resolution.
 */
static float round_to_one_percent(float rh)
{
    return roundf(rh);
}

/**
 * @brief Trigger a T+RH measurement and return humidity in % (rounded to 1%).
 */
static float read_humidity_percentage(void)
{
    float temperature_c = 0.0f;
    float humidity_rh = 0.0f;

    hdc1080_trigger_measurement_trh(HDC1080_I2C_ADDRESS);
    vTaskDelay(pdMS_TO_TICKS(20));
    hdc1080_read_temperature_humidity(HDC1080_I2C_ADDRESS, &temperature_c, &humidity_rh);

    (void)temperature_c; // temperature is not used here

    return round_to_one_percent(humidity_rh);
}

/**
 * @brief Measure humidity once and update internal bucket state.
 *
 * @return Last notified humidity in 5% bucket units (0..100).
 */
uint8_t measure_and_get_humidity(void)
{
    float humidity = read_humidity_percentage();
    uint8_t humidity_byte = convert_rh_to_byte(humidity);

    s_last_notified_humidity = to_bucket_5(humidity_byte);
    return s_last_notified_humidity;
}

/**
 * @brief Get last notified humidity bucket, measuring if necessary.
 *
 * @return 0–100 aralığında, 5'lik bucket'a yuvarlanmış nem değeri.
 */
uint8_t get_humidity(void)
{
    if (s_last_notified_humidity == 0xFF) {
        return measure_and_get_humidity();
    }

    return s_last_notified_humidity;
}

/**
 * @brief Initialize HDC1080 humidity sensor and set initial humidity state.
 *
 * - Konfigürasyon register'ını uygun çözünürlük ve moda ayarlar.
 * - İlk nem ölçümünü yapar ve last_notified_humidity değerini 5'lik bucket'a
 *   yuvarlanmış şekilde günceller.
 */
void initialize_humidity_sensor(void)
{
    ESP_LOGI(TAG, "Initializing HDC1080 humidity sensor.");

    // Default config (datasheet reset value 0x1000):
    // MODE=0, HRES=14bit, TRES=14bit
    HDC1080_ConfigReg cfg = {0};
    cfg.rst           = 0;                     // normal
    cfg.heat          = 0;                     // heater off
    cfg.mode          = HDC1080_MODE_BOTH;     // T + RH back-to-back
    cfg.btst          = 0;                     // write 0
    cfg.tres          = HDC1080_TRES_14BIT;    // max accuracy
    cfg.hres          = HDC1080_HRES_11BIT;    // speed/energy compromise
    cfg.reserved_low  = 0;
    cfg.reserved_high = 0;

    hdc1080_set_configuration(HDC1080_I2C_ADDRESS, cfg);

    float initial_humidity        = read_humidity_percentage();
    uint8_t initial_humidity_byte = convert_rh_to_byte(initial_humidity);
    s_last_notified_humidity        = to_bucket_5(initial_humidity_byte);

    ESP_LOGI(TAG,
             "Initial humidity: %.2f%% (bucket=%u%%)",
             initial_humidity,
             (unsigned)s_last_notified_humidity);
}

/**
 * @brief Periodic humidity read task.
 *
 * Davranış:
 * - 5 saniyede bir nem ölçümü yapar.
 * - Yeni %RH değeri önce 0–100 byte'a, sonra 5'lik bucket'a dönüşür.
 * - last_notified_humidity ile fark >= 5 ise callback tetiklenir.
 *
 * @param param FreeRTOS task parametresi (kullanılmıyor).
 */
void humidity_read_task(void *param)
{
    (void)param;

    while (1) {
        float humidity = read_humidity_percentage();
        uint8_t humidity_byte = convert_rh_to_byte(humidity);

        if (fabsf((float)s_last_notified_humidity - (float)humidity_byte) >= 5.0f) {
            if (s_humidity_update_callback) {
                s_last_notified_humidity = to_bucket_5(humidity_byte);
                s_humidity_update_callback(s_last_notified_humidity);
            } else {
                ESP_LOGW(TAG,
                         "Humidity changed but update callback is not set. "
                         "Skipping notification. current_bucket=%u",
                         (unsigned)to_bucket_5(humidity_byte));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
