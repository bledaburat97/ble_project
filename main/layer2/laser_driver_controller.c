#include "laser_driver_controller.h"

#include "../layer1/laser_driver_hw.h"
#include "../device_configuration.h"
#include "esp_log.h"

static const char *TAG = "LaserDriverController";

/**
 * Region → channel mapping.
 * Örneğin 6 bölge, her biri 6 kanal kullanıyor olabilir.
 * Bu tabloyu kendi projendeki gerçeğe göre doldur.
 */
static const LaserDriverInfo DRIVER_INFO_LIST[NUM_OF_LASER_DRIVERS] = {
    {
        .address = LP5036_ADDRESS_1, //u402 DRIVER
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 1,
                .led_list = 0x00000000000FFFFF, //SMD Lazer 1
            },
            {
                .region_id = 2,
                .led_list = 0x0000000FFFF00000, //SMD Lazer 2
            }
        }
    },
    {
        .address = LP5036_ADDRESS_2, //u401 DRIVER
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 3,
                .led_list = 0x0000000000003FFF, //SMD Lazer 1
            },
            {
                .region_id = 4,
                .led_list = 0x0000000FFF800000, //SMD Lazer 2
            }
        }
    },
    {
        .address = LP5036_ADDRESS_3, //u400 DRIVER
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 5,
                .led_list = 0x0000000000001FFF, //SMD Lazer 1
            },
            {
                .region_id = 6,
                .led_list = 0x0000000FFFC00000, //SMD Lazer 2
            }
        }
    }
};

/**
 * 0–100 brightness yüzdesini 0–255 PWM değerine dönüştür.
 */
static uint8_t brightness_percent_to_pwm(uint8_t percent)
{
    if (percent == 0)  return 0;
    if (percent >= 100) return 255;

    return (uint8_t)(((uint16_t)percent * 255U) / 100U);
}

void init_laser_driver(void)
{
    esp_err_t err = laser_hw_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Laser driver init failed: 0x%x", err);
        return;
    }

    // Başlangıçta tüm lazerler kapalı.
    laser_hw_set_global_enable(false);
}

/**
 * Region bazında parlaklık ayarı.
 * 
 * @param region_id   1..TOTAL_REGION_COUNT
 * @param brightness_percent 0..100
 */
void set_brightness_of_region(uint8_t region_id, uint8_t brightness_percent)
{
    if (region_id == 0 || region_id > TOTAL_REGION_COUNT) {
        ESP_LOGE(TAG, "Invalid region id: %u", region_id);
        return;
    }

    uint8_t pwm = brightness_percent_to_pwm(brightness_percent);

    for (uint8_t i = 0; i < NUM_OF_LASER_DRIVERS; i++) {
        const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[i];

        const RegionPiece *region_piece = get_region_piece_of_driver_by_id(region_id, i);
        if (region_piece == NULL) {
            ESP_LOGW(TAG, "Region %u not found on driver idx=%u", region_id, i);
            continue;
        }

        for (uint8_t led_index = 0; led_index < MAX_NUM_OF_LED_OF_LP5036; led_index++) {
            if ((region_piece->led_list >> led_index) & 0x01ULL) {
                if (laser_hw_set_channel_brightness(driver_info->address, led_index, &pwm) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to write for addr=0x%02X, led_index=%u", driver_info->address, led_index);
                }
            }
        }
    }
}

/**
 * Tüm region’ların parlaklığını ayarla.
 */
void set_brightness_of_all_regions(uint8_t brightness_percent)
{
    for (uint8_t region_id = 1; region_id <= TOTAL_REGION_COUNT; ++region_id) {
        set_brightness_of_region(region_id, brightness_percent);
    }
}

/**
 * Tüm lazerleri aç/kapat.
 * (Terapi state / helmet state iş kuralları üst katmanda kalmalı.)
 */
void set_laser_drivers_status(bool enabled)
{
    esp_err_t err = laser_hw_set_global_enable(enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set laser status=%d: 0x%x",
                 (int)enabled, err);
    }
}
