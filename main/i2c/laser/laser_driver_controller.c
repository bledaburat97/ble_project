#include "laser_driver_controller.h"

#include "../i2c_control.h"

#include "../../device_configuration.h"

#include "../../storage/log_types.h"

#include "esp_log.h"
#include "string.h"
#include "driver/gpio.h"

#define DEVICE_CONFIG0_REG 0x00
#define DEVICE_CONFIG1_REG 0x01
#define LED_CONFIG0_REG 0x02
#define LED_CONFIG1_REG 0x03
#define BANK_BRIGHTNESS_REG 0x04
#define OUT0_COLOR_REG 0x14
#define MAX_BRIGHTNESS 0xFF
#define MIN_BRIGHTNESS 0x00
#define LED0_BRIGHTNESS_REG 0x08

static const char *TAG = "LaserDriverControl";

static const LaserDriverInfo DRIVER_INFO_LIST[NUM_OF_LASER_DRIVERS] = {
    {
        .address = LP5036_ADDRESS_1, //u402 DRIVER
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 1,
                .led_list = 0x00000000000FFFFF, //SMD Lazer 1
                .is_bank = false
            },
            {
                .region_id = 2,
                .led_list = 0x0000000FFFF00000, //SMD Lazer 2
                .is_bank = false
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
                .is_bank = false
            },
            {
                .region_id = 4,
                .led_list = 0x0000000FFF800000, //SMD Lazer 2
                .is_bank = false
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
                .is_bank = false
            },
            {
                .region_id = 6,
                .led_list = 0x0000000FFFC00000, //SMD Lazer 2
                .is_bank = false
            }
        }
    }
      
    // 
    /*
    {
        .address = LP5036_ADDRESS_1, //u14 DRIVER
        .region_piece_count = 3,
        .region_piece_list = {
            {
                .region_id = 2,
                .led_list = 0x0000000000000004, //SMD Lazer 1
                .is_bank = false
            },
            {
                .region_id = 3,
                .led_list = 0x0000000000000001, //SMD Lazer 2
                .is_bank = false
            },
            {
                .region_id = 4,
                .led_list = 0x0000000000048000, // LEDler
                .is_bank = false
            }
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    },
    {
        .address = LP5036_ADDRESS_2, //u6 DRIVER
        .region_piece_count = 1,
        .region_piece_list = {
            {
                .region_id = 1,
                .led_list = 0x0000000A00000000, //33,35 TH Lazer
                .is_bank = false
            }
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    }
    */
        
};
/**
 * Bank modunda çalışan region parçaları için LED_CONFIG0/1 register’larını
 * doldurur. Şu an region_piece->is_bank true olan yapı bırakılmadığı için
 * aktifte yalnızca mapping hazır durumda.
 */
static void set_banked_leds(void)
{
    for (uint8_t i = 0; i < NUM_OF_LASER_DRIVERS; i++) {
        const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[i];

        for (uint8_t j = 0; j < driver_info->region_piece_count; j++) {
            const RegionPiece *region_piece = &driver_info->region_piece_list[j];

            if (region_piece == NULL) {
                ESP_LOGE(TAG, "Driver region is NULL (index=%u)", j);
                continue;
            }
            if (region_piece->region_id == 0) {
                continue;
            }

            if (!region_piece->is_bank) {
                continue;
            }

            uint16_t led_config = 0;

            for (uint8_t led_index = 0; led_index < MAX_NUM_OF_LED_OF_LP5036; led_index++) {
                if ((region_piece->led_list >> led_index) & 0x01U) {
                    if (led_index % 3 == 0) {
                        uint8_t bank_index = (uint8_t)(led_index / 3);
                        led_config |= (uint16_t)(1U << bank_index);
                    }
                }
            }

            if (led_config == 0) {
                continue;
            }

            uint8_t led_config0_data = (uint8_t)(led_config & 0xFFU);
            uint8_t led_config1_data = (uint8_t)((led_config >> 8) & 0xFFU);

            ESP_LOGI(TAG,
                     "Configuring banked LEDs: addr=0x%02X, LED_CONFIG0=0x%02X, LED_CONFIG1=0x%02X",
                     driver_info->address, led_config0_data, led_config1_data);

            if (write_register(driver_info->address,
                               LED_CONFIG0_REG,
                               &led_config0_data,
                               1) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write LED_CONFIG0_REG for address 0x%02X", driver_info->address);
            }


            if (write_register(driver_info->address,
                               LED_CONFIG1_REG,
                               &led_config1_data,
                               1) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write LED_CONFIG1_REG for address 0x%02X", driver_info->address);
            }

            ESP_LOGI(TAG, "Banked LED configuration completed for address 0x%02X", driver_info->address);

            break; // Bu drivern için bank region zaten bulundu
        }
    }
}

static const RegionPiece *get_region_piece_of_driver_by_id(uint8_t region_id, uint8_t driver_index)
{
    const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[driver_index];

    for (uint8_t i = 0; i < driver_info->region_piece_count; i++) {
        const RegionPiece *region = &driver_info->region_piece_list[i];
        if (region->region_id == region_id) {
            return region;
        }
    }

    return NULL;
}

/**
 * 0–100 arası parlaklık yüzdesini 0–255 arası register değerine çevirir.
 */
static uint8_t convert_brightness_percentage_to_brightness(const uint8_t brightness_percentage)
{
    if (brightness_percentage > 100U) {
        return MAX_BRIGHTNESS;
    }

    return (uint8_t)(((uint16_t)brightness_percentage * MAX_BRIGHTNESS) / 100U);
}

/**
 * Verilen region_id içindeki tüm LED’lerin parlaklığını ayarlar.
 * Region mapping, DRIVER_INFO_LIST tablosundan okunur.
 */
static void set_brightness_of_region(uint8_t region_id, const uint8_t brightness_percentage)
{
    if (region_id < 1U || region_id > TOTAL_REGION_COUNT) {
        ESP_LOGE(TAG, "Invalid region ID: %u", region_id);
        return;
    }

    uint8_t brightness = convert_brightness_percentage_to_brightness(brightness_percentage);
    ESP_LOGI(TAG, "Set brightness=%u (%%=%u) for region_id=%u",
             brightness, brightness_percentage, region_id);

    for (uint8_t i = 0; i < NUM_OF_LASER_DRIVERS; i++) {
        const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[i];

        const RegionPiece *region_piece = get_region_piece_of_driver_by_id(region_id, i);
        if (region_piece == NULL) {
            continue;
        }

        if (region_piece->is_bank) {
            if (write_register(driver_info->address,
                               BANK_BRIGHTNESS_REG,
                               &brightness,
                               1) != ESP_OK) {
                ESP_LOGE(TAG,
                         "Failed to write BANK_BRIGHTNESS_REG for address 0x%02X",
                         driver_info->address);
            } else {
                ESP_LOGI(TAG, "Bank brightness updated for driver 0x%02X", driver_info->address);
            }

        } else {
            for (uint8_t led_index = 0; led_index < MAX_NUM_OF_LED_OF_LP5036; led_index++) {
                if ((region_piece->led_list >> led_index) & 0x01U) {
                    uint8_t reg = (uint8_t)(OUT0_COLOR_REG + led_index);
                    if (write_register(driver_info->address,
                                       reg,
                                       &brightness,
                                       1) != ESP_OK) {
                        ESP_LOGE(TAG,
                                 "Failed to write OUT_COLOR_REG (0x%02X) for addr=0x%02X, led_index=%u",
                                 reg, driver_info->address, led_index);
                    }
                }
            }
        }
    }
}

void change_brightness(const uint8_t brightness[6]) {
    for (int i = 0; i < TOTAL_REGION_COUNT; i++) {
        set_brightness_of_region(i + 1, brightness[i]);
    }
}

/**
 * Tek bir LP5036 driver’ın DEVICE_CONFIG0_REG içindeki CHIP_EN bitini yönetir.
 */
static bool set_laser_driver_status(uint8_t laser_driver_index, bool status)
{
    if (laser_driver_index >= NUM_OF_LASER_DRIVERS) {
        return false;
    }

    uint8_t chip_en = status ? 0x40 : 0x00;
    const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[laser_driver_index];

    const int max_attempts = 3;
    esp_err_t ret = ESP_FAIL;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        ret = write_register(driver_info->address,
                             DEVICE_CONFIG0_REG,
                             &chip_en,
                             1);
        if (ret == ESP_OK) {
            break;
        }

        ESP_LOGW(TAG,
                 "Failed to write DEVICE_CONFIG0_REG for addr 0x%02X (status=%d), "
                 "attempt %d/%d, err=0x%x",
                 driver_info->address,
                 status,
                 attempt,
                 max_attempts,
                 ret);

    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "I2C write still failing for DEVICE_CONFIG0_REG (addr=0x%02X, status=%d)",
                 driver_info->address, status);
        return false;
    }

    if (!status) {
        uint8_t reg_val = 0;
        if (read_register(driver_info->address,
                          DEVICE_CONFIG0_REG,
                          &reg_val,
                          1) == ESP_OK) {

            if (reg_val & 0x40) {
                ESP_LOGE(TAG,
                         "CHIP_EN bit is still 1 after disable! addr=0x%02X, reg=0x%02X",
                         driver_info->address, reg_val);
                return false;
            }
        } else {
            ESP_LOGE(TAG,
                     "Failed to read-back DEVICE_CONFIG0_REG after disable, addr=0x%02X",
                     driver_info->address);
            return false;
        }
    }

    return true;
}

/**
 * Tüm LP5036 driver’ların CHIP_EN durumunu topluca aç/kapat.
 */
void set_laser_drivers_status(bool status)
{
    for (uint8_t driver_index = 0; driver_index < NUM_OF_LASER_DRIVERS; driver_index++) {
        set_laser_driver_status(driver_index, status);
    }
}

/**
 * LP5036 sürücülerini enable eden GPIO pinini output olarak konfigüre eder.
 */
void initialize_laser_driver_gpio(void)
{
    gpio_config_t io_conf_led_driver = {
        .pin_bit_mask = (1ULL << LED_DRIVER_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf_led_driver);
}

/**
 * LED_DRIVER_ENABLE_GPIO seviyesini ayarlar.
 */
void set_laser_drivers_gpio_pin_status(bool status)
{
    gpio_set_level(LED_DRIVER_ENABLE_GPIO, status);
}

/**
 * Tüm driver’larda DEVICE_CONFIG1_REG içindeki belirli bitleri (type) set/clear eder.
 */
void update_device_config1(bool status, DeviceConfig1UpdateType type)
{
    for (uint8_t driver_index = 0; driver_index < NUM_OF_LASER_DRIVERS; driver_index++) {
        const LaserDriverInfo *driver_info = &DRIVER_INFO_LIST[driver_index];

        uint8_t device_config1_value = 0;
        if (read_register(driver_info->address,
                          DEVICE_CONFIG1_REG,
                          &device_config1_value,
                          1) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read DEVICE_CONFIG1_REG for address 0x%02X",
                     driver_info->address);
            continue;
        }

        if (status) {
            device_config1_value |= (uint8_t)(1U << type);
        } else {
            device_config1_value &= (uint8_t)~(1U << type);
        }

        if (write_register(driver_info->address,
                           DEVICE_CONFIG1_REG,
                           &device_config1_value,
                           1) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write DEVICE_CONFIG1_REG for address 0x%02X",
                     driver_info->address);
        } else {
            ESP_LOGI(TAG,
                     "Updated DEVICE_CONFIG1_REG: addr=0x%02X, type=%d, status=%d",
                     driver_info->address, type, status);
        }
    }
}

/**
 * Lazer driver altyapısının başlangıç konfigürasyonu:
 * - Enable GPIO configure
 * - Driverları global enable hattı ile aç
 * - CHIP_EN bitlerini kapat
 * - Banked led konfigurasyonunu uygula
 */
void initialize_laser_drivers(void)
{
    initialize_laser_driver_gpio();
    set_laser_drivers_gpio_pin_status(true);
    set_laser_drivers_status(false);
    set_banked_leds();
}