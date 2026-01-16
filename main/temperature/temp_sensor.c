#include "temp_sensor.h"

#include "../i2c/i2c_control.h"
#include "../lp_core/lp_core_queue_manager.h"

#include "math.h"
#include "esp_log.h"

#define TEMPERATURE_REG      0x00
#define CONFIG_REG           0x01
#define LOW_THRESHOLD_REG    0x02
#define HIGH_THRESHOLD_REG   0x03

#define TEMPERATURE_RESOLUTION 0.0625f

static const char *TAG = "TemperatureSensor";

/**
 * Threshold sıcaklığını sensör formatına çevirir:
 * - 8 bit MSB: tam sayı kısmı
 * - MSB altındaki 4 bit: 0.0625 °C adımında fractional kısım
 */
static void convert_threshold_to_bytes(float threshold_in_degree,
                                       uint8_t *high_byte,
                                       uint8_t *low_byte)
{
    if (high_byte == NULL || low_byte == NULL) {
        return;
    }

    uint8_t integer_part = (uint8_t)threshold_in_degree;
    float fractional_part = threshold_in_degree - (float)integer_part;

    uint8_t fraction_bits = (uint8_t)roundf(fractional_part / TEMPERATURE_RESOLUTION);

    *high_byte = integer_part;
    *low_byte  = (uint8_t)((fraction_bits & 0x0F) << 4);
}

/**
 * Sensör formatındaki iki byte’ı float °C değerine çevirir.
 */
static float convert_bytes_to_float(uint8_t high_byte, uint8_t low_byte)
{
    float integer_part = (float)high_byte;

    uint8_t fraction_bits = (uint8_t)((low_byte >> 4) & 0x0F);
    float fractional_part = (float)fraction_bits * TEMPERATURE_RESOLUTION;

    return integer_part + fractional_part;
}

void temp_sensor_set_configuration(uint8_t device_address, TempSensorConfigReg config)
{
    uint8_t config_byte = *(uint8_t *)&config;

    write_register(device_address, CONFIG_REG, &config_byte, 1);

    // Opsiyonel read-back (şu an sadece okunuyor, doğrulama yapılmıyor)
    uint8_t configuration_bytes[2] = {0};
    (void)read_register(device_address, CONFIG_REG, configuration_bytes, 2);
}

/**
 * LOW / HIGH threshold register’larını verilen sıcaklık değerine göre ayarlar.
 */
void temp_sensor_set_threshold(uint8_t device_address,
                               float threshold_in_degree,
                               TemperatureThresholdType type)
{
    uint8_t msb = 0;
    uint8_t lsb = 0;

    convert_threshold_to_bytes(threshold_in_degree, &msb, &lsb);

    uint8_t threshold[2] = { msb, lsb };

    if (type == LOW) {
        ESP_LOGI(TAG, "New low temperature limit: %.2f°C", threshold_in_degree);
        write_register(device_address, LOW_THRESHOLD_REG, threshold, 2);
    } else if (type == HIGH) {
        ESP_LOGI(TAG, "New high temperature limit: %.2f°C", threshold_in_degree);
        write_register(device_address, HIGH_THRESHOLD_REG, threshold, 2);
    }
}

/**
 * Verilen sensörden sıcaklık ölçer ve float °C cinsinden döndürür.
 */
float temp_sensor_read(uint8_t device_address)
{
    uint8_t temperature_bytes[2] = {0};

    read_register(device_address, TEMPERATURE_REG, temperature_bytes, 2);

    float temperature = convert_bytes_to_float(temperature_bytes[0], temperature_bytes[1]);
    return temperature;
}
