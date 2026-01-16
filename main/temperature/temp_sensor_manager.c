#include "temp_sensor_manager.h"

#include "temp_measurement_setter.h"
#include "temp_alert_setter.h"
#include "temp_sensor_reader.h"

#include "temp_sensor_reader_internal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "temp_sensor_config.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "TempSensorManager";

static void (*s_temp_update_cb)(uint8_t) = NULL;
static void (*s_temp_alert_cb)(uint8_t)  = NULL;

static float round_down_to_half(float t) { return floorf(t * 2.0f) / 2.0f; }

static void on_alert(uint8_t idx)
{
    // ilk kez mi alerted oluyoruz?
    bool was_any = temp_sensor_reader_is_any_alerted_sensor();

    // threshold’ları alert moduna çek
    temp_measurement_setter_set_alert_thresholds(idx);

    // mask update
    temp_sensor_reader_internal_add_alerted(idx);

    // sadece ilk alert’te üst katmanı haberdar et
    if (s_temp_alert_cb && !was_any) s_temp_alert_cb(idx);
}

static void on_normal(uint8_t idx)
{
    temp_sensor_reader_internal_remove_alerted(idx);
    temp_measurement_setter_set_normal_thresholds(idx);
}

void temp_sensor_manager_register_temperature_update(void (*cb)(uint8_t))
{
    s_temp_update_cb = cb;
}

void temp_sensor_manager_register_temp_alert(void (*cb)(uint8_t))
{
    s_temp_alert_cb = cb;
}

void temp_sensor_manager_initialize(void)
{
    ESP_LOGI(TAG, "Temp module init (manager)");

    temp_sensor_reader_internal_reset();

    AlertPolarity alert_polarity = ALERT_ACTIVE_HIGH;
    temp_measurement_setter_initialize_sensors(alert_polarity);
    temp_alert_setter_register_temperature_alert(on_alert);
    temp_alert_setter_register_temperature_normal(on_normal);

    bool normal_pin_status = (alert_polarity != ALERT_ACTIVE_HIGH); 
    temp_alert_setter_initialize(normal_pin_status);
}

void temp_sensor_manager_temperature_read_task(void *param)
{
    (void)param;

    while (1) {
        float avg = temp_measurement_setter_measure_average_temperature();
        float rounded = round_down_to_half(avg);
        uint8_t b = temp_measurement_setter_convert_float_to_byte(rounded);

        uint8_t last = temp_sensor_reader_internal_get_cached();

        // ilk tur: cache doldur
        if (last == 0xFF) {
            temp_sensor_reader_internal_set_cached(b);
            vTaskDelay(pdMS_TO_TICKS(TEMP_READ_PERIOD_MS));
            continue;
        }

        int diff = (int)last - (int)b;
        if (diff < 0) diff = -diff;

        if (diff >= 2) {
            temp_sensor_reader_internal_set_cached(b);
            if (s_temp_update_cb) s_temp_update_cb(b);
        }

        vTaskDelay(pdMS_TO_TICKS(TEMP_READ_PERIOD_MS));
    }
}

void temp_sensor_manager_alert_monitor_task(void *param)
{
    temp_alert_setter_monitor_alert_task(param);
}
