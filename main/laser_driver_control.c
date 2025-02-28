#include "laser_driver_control.h"
#include "i2c_control.h"
#include "esp_log.h"
#include "string.h"


#define LP5036_ADDRESS_1 0x30   // I2C address for the first LP5036
#define LP5036_ADDRESS_2 0x31   // I2C address for the second LP5036
#define LP5036_ADDRESS_3 0x30   // I2C address for the third LP5036

#define DEVICE_CONFIG0_REG 0x00
#define DEVICE_CONFIG1_REG 0x01
#define POWER_SAVE_EN_BIT_INDEX 6
#define LED_CONFIG0_REG 0x02
#define LED_CONFIG1_REG 0x03
#define BANK_A_COLOR_REG 0x05
#define OUT0_COLOR_REG 0x14
#define MAX_BRIGHTNESS 0xFF
#define MIN_BRIGHTNESS 0x00

#define NUM_OF_LP5036 3
static const char *LASER_TAG = "LaserDriverControl";

static const LP5036Info lp5036Infos[NUM_OF_LP5036] = {
    {
        .address = LP5036_ADDRESS_1,
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 1,
                .led_list = 0x000000000000003F, //1,2,3,4,5,6
                .is_bank = true
            },
            {
                .region_id = 4,
                .led_list = 0x00000000000000C0, //7,8
                .is_bank = false
            }
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    },
    {
        .address = LP5036_ADDRESS_2,
        .region_piece_count = 2,
        .region_piece_list = {
            {
                .region_id = 2,
                .led_list = 0x000000000000038, //4,5,6
                .is_bank = true
            },
            {
                .region_id = 4,
                .led_list = 0x000000000000007, //1,2,3
                .is_bank = false
            },
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    },
    {
        .address = LP5036_ADDRESS_3,
        .region_piece_count = 1,
        .region_piece_list = {
            {
                .region_id = 3,
                .led_list = 0x000000000000007, //1,2,3
                .is_bank = true
            }
        },
        .i2c_master_num = I2C_SECOND_MASTER_NUM
    }
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
                    if (write_register(info->address, LED_CONFIG0_REG, &led_config0_data, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write LED_CONFIG0_REG for address 0x%02X", info->address);
                    }
                    if (write_register(info->address, LED_CONFIG1_REG, &led_config1_data, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write LED_CONFIG1_REG for address 0x%02X", info->address);
                    }
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

static void set_brightness_of_region(uint8_t region_id, uint8_t brightness){
    if (region_id < 1 || region_id > TOTAL_REGION_COUNT) {
        ESP_LOGE(LASER_TAG, "Invalid region ID: %d", region_id);
        return;
    }

    for(uint8_t i = 0; i < NUM_OF_LP5036; i++)
    {
        const LP5036Info *info = &lp5036Infos[i];

        const RegionPiece *region_piece = get_region_piece_of_driver_by_id(region_id, info);

        if (region_piece == NULL) {
            ESP_LOGE(LASER_TAG, "DriverRegion with ID %d not found", region_id);
            continue;
        }

        if(region_piece->is_bank)
        {
            if (write_register(info->address, BANK_A_COLOR_REG, &brightness, 1, info->i2c_master_num) != ESP_OK) {
                ESP_LOGE(LASER_TAG, "Failed to write BANK_A_COLOR_REG brightness for address 0x%02X", info->address);
            }
        }
        else
        {
            for (uint8_t j = 0; j < MAX_NUM_OF_LED_OF_LP5036; j++) {
                if ((region_piece->led_list >> j) & 1) {
                    if (write_register(info->address, OUT0_COLOR_REG + j, &brightness, 1, info->i2c_master_num) != ESP_OK) {
                        ESP_LOGE(LASER_TAG, "Failed to write OUT0_COLOR_REG brightness for address 0x%02X, for led index %d", info->address, j);
                    }
                }
            }
        }
    }
}

static void set_laser_drivers_status(bool status)
{
    for(uint8_t lp5036_index = 0; lp5036_index < NUM_OF_LP5036; lp5036_index++){
        uint8_t chip_en = status ? 0x40 : 0x00;
        if (write_register(lp5036Infos[lp5036_index].address, DEVICE_CONFIG0_REG, &chip_en, 1, lp5036Infos[lp5036_index].i2c_master_num) != ESP_OK) {
            ESP_LOGE(LASER_TAG, "Failed to write DEVICE_CONFIG0_REG for address 0x%02X, for status: %d", lp5036Infos[lp5036_index].address, status);
        }
    }
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
            ESP_LOGI(LASER_TAG, "Successfully updated POWER_SAVE_EN for address 0x%02X to %d", lp5036Infos[lp5036_index].address, status);
        }
    }
}

void set_brightness(RegionStatusChangedInfo *region_status_changed_infos, uint8_t num_of_changed_regions)
{
    for(uint8_t i = 0; i < num_of_changed_regions; i++){
        set_brightness_of_region(region_status_changed_infos[i].region_id, region_status_changed_infos[i].brightness);
    }
}

void initialize_laser_drivers() 
{
    set_laser_drivers_status(true);
    set_banked_leds();
}


void stop_laser_drivers() 
{
    set_laser_drivers_status(false);
}
