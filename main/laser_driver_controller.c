#include "laser_driver_controller.h"
#include "i2c_control.h"
#include "esp_log.h"
#include "string.h"
#include "driver/gpio.h"
#include "state_manager.h"
#include "device_configuration.h"

#define DEVICE_CONFIG0_REG 0x00
#define DEVICE_CONFIG1_REG 0x01
#define LED_CONFIG0_REG 0x02
#define LED_CONFIG1_REG 0x03
#define BANK_BRIGHTNESS_REG 0x04
#define OUT0_COLOR_REG 0x14
#define MAX_BRIGHTNESS 0xFF
#define MIN_BRIGHTNESS 0x00
#define LED0_BRIGHTNESS_REG 0x08

static const char *LASER_TAG = "LaserDriverControl";

static LP5036Info lp5036Infos[NUM_OF_LP5036];

static const LP5036Info not_banked_test_lp5036Infos[NUM_OF_LP5036] = {
    
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
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
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
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
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
                .led_list = 0x0000000FFFA00000, //SMD Lazer 2
                .is_bank = false
            }
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    }
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

static void set_banked_leds() {
    for (uint8_t i = 0; i < NUM_OF_LP5036; i++) {
        const LP5036Info *info = &lp5036Infos[i];

        for (uint8_t j = 0; j < info->region_piece_count; j++) {
            const RegionPiece *region_piece = &info->region_piece_list[j];

            if (region_piece == NULL) {
                ESP_LOGE(LASER_TAG, "DriverRegion is not found");
                continue;
            }
            if (region_piece->region_id == 0) {
                continue;
            }
            uint16_t led_config = 0;

            if (region_piece->is_bank) {
                for (uint8_t led_index = 0; led_index < MAX_NUM_OF_LED_OF_LP5036; led_index++) {
                    if ((region_piece->led_list >> led_index) & 1) {
                        if(led_index % 3 == 0) {
                            uint8_t x = led_index / 3;
                            led_config |= (1 << x);
                        }
                    }
                }

                if (led_config != 0) {
                    uint8_t led_config0_data = led_config & 0xFF;
                    uint8_t led_config1_data = (led_config >> 8) & 0xFF;
                    
                    ESP_LOGI(LASER_TAG, "set_banked_leds write_register() called with:");
                    ESP_LOGI(LASER_TAG, "  device_address: 0x%02X", info->address);
                    ESP_LOGI(LASER_TAG, "  reg_address: 0x%02X", LED_CONFIG0_REG);
                    ESP_LOGI(LASER_TAG, "  led_config0_data: 0x%02X", led_config0_data);
                    ESP_LOGI(LASER_TAG, "  led_config1_data: 0x%02X", led_config1_data);
                    ESP_LOGI(LASER_TAG, "  length: %d", 1);
                    ESP_LOGI(LASER_TAG, "  i2c_master_number: %d", info->i2c_master_num);

                    if (write_register(info->address, LED_CONFIG0_REG, &led_config0_data, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write LED_CONFIG0_REG for address 0x%02X", info->address);
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));

                    if (write_register(info->address, LED_CONFIG1_REG, &led_config1_data, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write LED_CONFIG1_REG for address 0x%02X", info->address);
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                    ESP_LOGI(LASER_TAG, "LED CONFIG is done");
                }
                break;
            }
        }
    }
}

static const RegionPiece* get_region_piece_of_driver_by_id(uint8_t region_id, const LP5036Info *info) {
        
    for (uint8_t i = 0; i < info->region_piece_count; i++) {
        const RegionPiece *region = &info->region_piece_list[i];
        if (region->region_id == region_id) {
            return &info->region_piece_list[i];
        }
    }
    return NULL;
}

static uint8_t convert_brightness_percentage_to_brightness(uint8_t brightness_percentage)
{
    //ESP_LOGI(LASER_TAG, "Brightness percentage: %u", brightness_percentage);
    if (brightness_percentage > 20)
    {
        brightness_percentage = 20;
    }
    return (uint8_t)(((uint16_t)brightness_percentage * 255) / 20);
}

void set_brightness_of_region(uint8_t region_id, uint8_t brightness_percentage)
{
    uint8_t brightness = convert_brightness_percentage_to_brightness(brightness_percentage);
    //ESP_LOGI(LASER_TAG, "brightness: %u", brightness);
    if (region_id < 1 || region_id > TOTAL_REGION_COUNT) {
        ESP_LOGE(LASER_TAG, "Invalid region ID: %d", region_id);
        return;
    }

    for(uint8_t i = 0; i < NUM_OF_LP5036; i++)
    {
        const LP5036Info *info = &lp5036Infos[i];

        const RegionPiece *region_piece = get_region_piece_of_driver_by_id(region_id, info);

        if (region_piece == NULL) {
            //ESP_LOGI(LASER_TAG, "DriverRegion with ID %d not found", region_id);
            continue;
        }


        if(region_piece->is_bank)
        {
            if (write_register(info->address, BANK_BRIGHTNESS_REG, &brightness, 1, info->i2c_master_num) != ESP_OK) {
                ESP_LOGE(LASER_TAG, "Failed to write BANK_BRIGHTNESS brightness for address 0x%02X", info->address);
            }
            ESP_LOGI(LASER_TAG, "Banked leds are running.");

            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            //TODO: OUT0 parlaklık set ediyorsa:
            for (uint8_t j = 0; j < MAX_NUM_OF_LED_OF_LP5036; j++) {
                if ((region_piece->led_list >> j) & 1) {
                    if (write_register(info->address, OUT0_COLOR_REG + j, &brightness, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write OUT0_COLOR_REG brightness for address 0x%02X, for led index %d", info->address, j);
                    }
                    //vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            //ESP_LOGI(LASER_TAG, "Individual leds are running.");
        }
        
    }
}

static void set_laser_driver_status(uint8_t laser_driver_index, bool status) {
    if(laser_driver_index >= NUM_OF_LP5036){
        return;
    } 
    uint8_t chip_en = status ? 0x40 : 0x00;
    //ESP_LOGI(LASER_TAG, "set_laser_drivers_status write_register() called with:");
    //ESP_LOGI(LASER_TAG, "  device_address: 0x%02X", lp5036Infos[laser_driver_index].address);
    //ESP_LOGI(LASER_TAG, "  reg_address: 0x%02X", DEVICE_CONFIG0_REG);
    //ESP_LOGI(LASER_TAG, "  data: 0x%02X", chip_en);
    //ESP_LOGI(LASER_TAG, "  length: %d", 1);
    //ESP_LOGI(LASER_TAG, "  i2c_master_number: %d", lp5036Infos[laser_driver_index].i2c_master_num);
    if (write_register(lp5036Infos[laser_driver_index].address, DEVICE_CONFIG0_REG, &chip_en, 1, lp5036Infos[laser_driver_index].i2c_master_num) != ESP_OK) {
        ESP_LOGE(LASER_TAG, "Failed to write DEVICE_CONFIG0_REG for address 0x%02X, for status: %d", lp5036Infos[laser_driver_index].address, status);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); 
}

void set_laser_drivers_status(bool status)
{
    for(uint8_t lp5036_index = 0; lp5036_index < NUM_OF_LP5036; lp5036_index++){
        set_laser_driver_status(lp5036_index, status);
    }
}

void initialize_laser_driver_gpio(){
    gpio_config_t io_conf_led_driver = {
        .pin_bit_mask = (1ULL << LED_DRIVER_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf_led_driver);
    /*
    ESP_LOGI(LASER_TAG, "led Gpio is initialized successfully.");

        gpio_config_t io_conf_laser_driver = {
        .pin_bit_mask = (1ULL << LASER_DRIVER_ENABLE_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf_laser_driver);
    */
    ESP_LOGI(LASER_TAG, "laser Gpio is initialized successfully.");
}

void set_laser_drivers_gpio_pin_status(bool status) {
    gpio_set_level(LED_DRIVER_ENABLE_GPIO, status);
    //gpio_set_level(LASER_DRIVER_ENABLE_GPIO, status);

    ESP_LOGI(LASER_TAG, "Gpio pin status is set.");
}

void update_device_config1(bool status, DeviceConfig1UpdateType type) {

    for(uint8_t lp5036_index = 0; lp5036_index < NUM_OF_LP5036; lp5036_index++)
    {
        uint8_t device_config1_result;
        if (read_register(lp5036Infos[lp5036_index].address, DEVICE_CONFIG1_REG, &device_config1_result, 1, lp5036Infos[lp5036_index].i2c_master_num) != ESP_OK) {
            ESP_LOGE(LASER_TAG, "Failed to read DEVICE_CONFIG1_REG for address 0x%02X", lp5036Infos[lp5036_index].address);
            continue;
        }
        
        if (status) {
            device_config1_result |= (1 << type);
        } else {
            device_config1_result &= ~(1 << type);
        }

        // Write the modified value back to DEVICE_CONFIG1_REG
        if (write_register(lp5036Infos[lp5036_index].address, DEVICE_CONFIG1_REG, &device_config1_result, 1, lp5036Infos[lp5036_index].i2c_master_num) != ESP_OK) {
            ESP_LOGE(LASER_TAG, "Failed to write DEVICE_CONFIG1_REG for address 0x%02X", lp5036Infos[lp5036_index].address);
        } else {
            ESP_LOGI(LASER_TAG, "Successfully updated type %d for address 0x%02X to %d", lp5036Infos[lp5036_index].address, type, status);
        }
    }
}

void initialize_laser_drivers() 
{
    memcpy(lp5036Infos, not_banked_test_lp5036Infos, sizeof(LP5036Info) * NUM_OF_LP5036);

    initialize_laser_driver_gpio();
    set_laser_drivers_gpio_pin_status(true);
    set_laser_drivers_status(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    set_banked_leds();
}