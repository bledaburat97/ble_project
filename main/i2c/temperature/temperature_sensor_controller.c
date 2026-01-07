#include "temperature_sensor_controller.h"

#include "temperature_sensor.h"
#include "temperature_alert_controller.h"

#include "../i2c_control.h"
#include "../../device_configuration.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include <math.h>

static const char *TAG = "TemperatureSensorController";

/**
 * Kullanılan sıcaklık sensörlerinin I2C adresleri.
 */
static const uint8_t SENSOR_ADDRESS_LIST[] = {
    SECOND_PJ85775_ADDRESS,
    FIRST_PJ85775_ADDRESS,
    THIRD_PJ85775_ADDRESS
};

static uint8_t s_last_notified_temperature_byte = 0xFF;
static void (*s_temp_update_callback)(uint8_t) = NULL;
static void (*s_temp_alert_callback)(uint8_t) = NULL;

static uint8_t s_alerted_sensor_mask = 0x00;

static bool is_valid_sensor_index(uint8_t sensor_index)
{
    return sensor_index < TEMPERATURE_SENSOR_COUNT;
}

static void add_alerted_sensor(uint8_t sensor_index)
{
    if (!is_valid_sensor_index(sensor_index)) {
        ESP_LOGE(TAG, "Invalid sensor index in alert: %u", sensor_index);
        return;
    }
    s_alerted_sensor_mask |= (uint8_t)(1U << sensor_index);
}

static void remove_alerted_sensor(uint8_t sensor_index)
{
    if (!is_valid_sensor_index(sensor_index)) {
        ESP_LOGE(TAG, "Invalid sensor index in normal: %u", sensor_index);
        return;
    }
    s_alerted_sensor_mask &= (uint8_t)~(1U << sensor_index);
}

bool is_any_alerted_sensor(void)
{
    return s_alerted_sensor_mask != 0x00;
}

/**
 * 0–64 °C aralığındaki float sıcaklığı 1 byte’a sıkıştırır.
 * - İlk 6 bit: tam sayı (0–63)
 * - Son 2 bit: 0, 0.25, 0.5, 0.75 adımları
 * Aralık dışı değerlerde 0xFF döner.
 */
static uint8_t convert_float_to_byte(float temperature)
{
    // 6 bit tam sayı (0 - 63), son iki bit (0, 0.25, 0.5, 0.75)
    if (temperature < 0.0f || temperature > 64.0f) {
        return 0xFF;
    }

    uint8_t integer_part   = (uint8_t)temperature;
    float fractional_part  = temperature - (float)integer_part;

    uint8_t frac_bits;
    if (fractional_part < 0.25f) {
        frac_bits = 0b00;
    } else if (fractional_part < 0.5f) {
        frac_bits = 0b01;
    } else if (fractional_part < 0.75f) {
        frac_bits = 0b10;
    } else {
        frac_bits = 0b11;
    }

    uint8_t temperature_in_byte = (uint8_t)((integer_part << 2) | frac_bits);
    return temperature_in_byte;
}

void register_temperature_update(void (*callback)(uint8_t))
{
    s_temp_update_callback = callback;
}

/**
 * Tüm sensörlerden sıcaklık okuyup ortalamayı döndürür.
 */
static float measure_average_temperature(void)
{
    float sum_of_temperatures = 0.0f;

    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        float temperature_in_degree = read_temperature_of_sensor(SENSOR_ADDRESS_LIST[i]);
        sum_of_temperatures += temperature_in_degree;
    }

    return sum_of_temperatures / (float)TEMPERATURE_SENSOR_COUNT;
}

uint8_t measure_and_get_temperature(void)
{
    float average_temperature = measure_average_temperature();
    s_last_notified_temperature_byte = convert_float_to_byte(average_temperature);
    return s_last_notified_temperature_byte;
}

uint8_t get_temperature(void)
{
    if (s_last_notified_temperature_byte == 0xFF) {
        return measure_and_get_temperature();
    }
    return s_last_notified_temperature_byte;
}

/**
 * Sıcaklığı aşağıya doğru 0.5 °C hassasiyetle yuvarlar.
 * Örnek: 37.8 → 37.5
 */
static float round_down_to_half(float temp)
{
    return floorf(temp * 2.0f) / 2.0f;
}

/**
 * Periyodik sıcaklık okuma task’i:
 * - Tüm sensörlerden ortalama sıcaklığı okur
 * - 0.5 °C gridine yuvarlar
 * - Son bildirilen değerden en az 2 “step” (0.5 °C step → 1 byte step) değişim varsa callback tetikler
 */
void temperature_read_task(void *param)
{
    (void)param;

    while (1) {
        float average_temperature     = measure_average_temperature();
        float rounded_temperature     = round_down_to_half(average_temperature);
        uint8_t rounded_temperature_byte = convert_float_to_byte(rounded_temperature);

        int diff = (int)s_last_notified_temperature_byte - (int)rounded_temperature_byte;
        if (diff < 0) {
            diff = -diff;
        }

        if (diff >= 2) {
            if (s_temp_update_callback) {
                s_last_notified_temperature_byte = rounded_temperature_byte;
                s_temp_update_callback(s_last_notified_temperature_byte);
            } else {
                ESP_LOGE(TAG, "Temperature update callback is not registered, update skipped.");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * Alert moduna geçtiğinde sensör eşiklerini:
 * - LOW_THRESHOLD_IN_ALERT
 * - HIGH_THRESHOLD_IN_ALERT
 * değerlerine çeker.
 *
 * HIGH_THRESHOLD_IN_ALERT yüksek seçilir; alert moddayken yeniden alert tetiklenmesini engellemek için
 * sadece düşük threshold üzerinden “normale dönüş” beklenir.
 */
static void increase_thresholds(uint8_t sensor_index)
{
    set_threshold_temperature(SENSOR_ADDRESS_LIST[sensor_index], LOW_THRESHOLD_IN_ALERT, LOW);
    set_threshold_temperature(SENSOR_ADDRESS_LIST[sensor_index], HIGH_THRESHOLD_IN_ALERT, HIGH);
}

/**
 * Normal moda döndüğünde eşikleri:
 * - LOW_THRESHOLD_IN_NORMAL
 * - HIGH_THRESHOLD_IN_NORMAL
 * değerlerine çeker.
 *
 * HIGH_THRESHOLD_IN_NORMAL: alert’e geçiş eşiği  
 * LOW_THRESHOLD_IN_ALERT: alert’ten normale dönmek için inilmesi gereken eşik  
 * HIGH_THRESHOLD_IN_ALERT: alert modda yeniden alert tetiklenmesin diye yüksek tutulur.
 */
static void set_normal_thresholds(uint8_t sensor_index)
{
    set_threshold_temperature(SENSOR_ADDRESS_LIST[sensor_index], LOW_THRESHOLD_IN_NORMAL, LOW);
    set_threshold_temperature(SENSOR_ADDRESS_LIST[sensor_index], HIGH_THRESHOLD_IN_NORMAL, HIGH);
}

/**
 * Sensörlerden birinde sıcaklık alert eşiklerini geçtiğinde çağrılır.
 * - Ortalamayı okuyup loglar
 * - Eşikleri alert (histerezis) setine çeker
 * - Üst katmana sensör index’i ile alert callback’i yollar
 */
static void on_temp_alert_callback(uint8_t sensor_index)
{
    float average_temperature = measure_average_temperature();
    float rounded_temperature = round_down_to_half(average_temperature);

    ESP_LOGI(TAG, "Temperature ALERT from sensor index=%u", sensor_index);
    ESP_LOGI(TAG, "Average temperature: %.2f°C", average_temperature);
    ESP_LOGI(TAG, "Saved (rounded) temperature: %.2f°C", rounded_temperature);

    increase_thresholds(sensor_index);

    if (s_temp_alert_callback && !is_any_alerted_sensor()) {
        s_temp_alert_callback(sensor_index);
    }

    add_alerted_sensor(sensor_index);
}

/**
 * Sensör sıcaklığı alert aralığından çıkıp normal banda döndüğünde çağrılır.
 * - Ortalamayı okuyup loglar
 * - Eşikleri normal moda çeker
 */
static void on_temp_normal_callback(uint8_t sensor_index)
{
    float average_temperature = measure_average_temperature();
    float rounded_temperature = round_down_to_half(average_temperature);

    ESP_LOGI(TAG, "Temperature NORMAL from sensor index=%u", sensor_index);
    ESP_LOGI(TAG, "Saved (rounded) temperature: %.2f°C", rounded_temperature);

    remove_alerted_sensor(sensor_index);

    set_normal_thresholds(sensor_index);
}

void register_temp_alert_callback(void (*callback)(uint8_t))
{
    s_temp_alert_callback = callback;
}

/**
 * Tüm sıcaklık sensörlerini başlatır:
 * - Konfig register’ını yazar
 * - Alert / normal callback’lerini register eder
 * - Normal eşikleri atar (LOW_THRESHOLD_IN_NORMAL / HIGH_THRESHOLD_IN_NORMAL)
 */
void initialize_temperature_sensor(void)
{
    ESP_LOGI(TAG, "Initialize temperature sensors.");

    TempSensorConfigReg config;
    config.os   = OS_DISABLED;
    config.cr   = CR_4HZ;
    config.fq   = FAULT_QUEUE_2;
    config.pol  = ALERT_ACTIVE_HIGH;
    config.altm = ALERT_COMPARATOR_MODE;
    config.sd   = NORMAL_MODE;

    set_alert_pin_normal_status(config.pol != ALERT_ACTIVE_HIGH);

    set_active_temp_sensor_count(TEMPERATURE_SENSOR_COUNT);

    s_alerted_sensor_mask = 0x00;

    register_temperature_alert(on_temp_alert_callback);
    register_temperature_normal(on_temp_normal_callback);

    for (int i = 0; i < TEMPERATURE_SENSOR_COUNT; i++) {
        set_configuration(SENSOR_ADDRESS_LIST[i], config);
        set_normal_thresholds((uint8_t)i);
    }
}
