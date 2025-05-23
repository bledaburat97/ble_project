#include "laser_driver_control.h"
#include "i2c_control.h"
#include "esp_log.h"
#include "string.h"
#include "driver/gpio.h"
#include "state_manager.h"

#define LP5036_ADDRESS_1 0x30   // I2C address for the first LP5036
#define LP5036_ADDRESS_2 0x31   // I2C address for the second LP5036
#define LP5036_ADDRESS_3 0x30   // I2C address for the third LP5036

#define DEVICE_CONFIG0_REG 0x00
#define DEVICE_CONFIG1_REG 0x01
#define LED_CONFIG0_REG 0x02
#define LED_CONFIG1_REG 0x03
#define BANK_A_COLOR_REG 0x05
#define OUT0_COLOR_REG 0x14
#define MAX_BRIGHTNESS 0xFF
#define MIN_BRIGHTNESS 0x00

#define NUM_OF_LP5036 1
static const char *LASER_TAG = "LaserDriverControl";
const int LASER_DRIVER_GPIO[NUM_OF_LP5036] = { GPIO_NUM_4};

static const LP5036Info lp5036Infos[NUM_OF_LP5036] = {
    {
        .address = LP5036_ADDRESS_1,
        .region_piece_count = 3,
        .region_piece_list = {
            {
                .region_id = 1,
                .led_list = 0x0000000000000007, //1,2,3,
                .is_bank = true
            },
            {
                .region_id = 2,
                .led_list = 0x0000000000000060, //6,7
                .is_bank = false
            },
            {
                .region_id = 3,
                .led_list = 0x000000000000180, //8,9
                .is_bank = false
            }
        },
        .i2c_master_num = I2C_FIRST_MASTER_NUM
    },
    /*
    {
        .address = LP5036_ADDRESS_2, //LP5036_ADDRESS_2
        .region_piece_count = 1,
        .region_piece_list = {
            {
                .region_id = 3,
                .led_list = 0x000000000000180, //8,9
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
                    ESP_LOGI(LASER_TAG, "  data: 0x%02X", led_config0_data);
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
    ESP_LOGI(LASER_TAG, "Brightness percentage: %u", brightness_percentage);
    if (brightness_percentage > 20)
    {
        brightness_percentage = 20;
    }
    return (uint8_t)(((uint16_t)brightness_percentage * 255) / 20);
}

void set_brightness_of_region(uint8_t region_id, uint8_t brightness_percentage)
{
    uint8_t brightness = convert_brightness_percentage_to_brightness(brightness_percentage);
    
    if (region_id < 1 || region_id > TOTAL_REGION_COUNT) {
        ESP_LOGE(LASER_TAG, "Invalid region ID: %d", region_id);
        return;
    }

    for(uint8_t i = 0; i < NUM_OF_LP5036; i++)
    {
        const LP5036Info *info = &lp5036Infos[i];

        const RegionPiece *region_piece = get_region_piece_of_driver_by_id(region_id, info);

        if (region_piece == NULL) {
            ESP_LOGI(LASER_TAG, "DriverRegion with ID %d not found", region_id);
            continue;
        }

        /*TODO: 
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
        */
    }
}

static void set_laser_drivers_status(bool status)
{
    for(uint8_t lp5036_index = 0; lp5036_index < NUM_OF_LP5036; lp5036_index++){
        uint8_t chip_en = status ? 0x40 : 0x00;
        vTaskDelay(pdMS_TO_TICKS(100)); 
        ESP_LOGI(LASER_TAG, "set_laser_drivers_status write_register() called with:");
        ESP_LOGI(LASER_TAG, "  device_address: 0x%02X", lp5036Infos[0].address);
        ESP_LOGI(LASER_TAG, "  reg_address: 0x%02X", DEVICE_CONFIG0_REG);
        ESP_LOGI(LASER_TAG, "  data: 0x%02X", chip_en);
        ESP_LOGI(LASER_TAG, "  length: %d", 1);
        ESP_LOGI(LASER_TAG, "  i2c_master_number: %d", lp5036Infos[0].i2c_master_num);
        if (write_register(lp5036Infos[0].address, DEVICE_CONFIG0_REG, &chip_en, 1, lp5036Infos[0].i2c_master_num) != ESP_OK) {
            ESP_LOGE(LASER_TAG, "Failed to write DEVICE_CONFIG0_REG for address 0x%02X, for status: %d", lp5036Infos[0].address, status);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}

static void initialize_laser_driver_gpio(){
    gpio_config_t io_conf_laser_driver = {
        .pin_bit_mask = (1ULL << LASER_DRIVER_GPIO[0]) ,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    for (int i = 0; i < NUM_OF_LP5036; i++) {
        io_conf_laser_driver.pin_bit_mask = (1ULL << LASER_DRIVER_GPIO[i]);
        gpio_config(&io_conf_laser_driver);
    }
    ESP_LOGI(LASER_TAG, "Gpio is initialized successfully.");
}

static void set_laser_drivers_gpio_pin_status(bool status) {
    for(int laser_driver_index = 0; laser_driver_index < NUM_OF_LP5036; laser_driver_index++) {
        gpio_set_level(LASER_DRIVER_GPIO[laser_driver_index], status);
    }
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
            ESP_LOGI(LASER_TAG, "Successfully updated POWER_SAVE_EN for address 0x%02X to %d", lp5036Infos[lp5036_index].address, status);
        }
    }
}

/*
void set_brightness(RegionStatusChangedInfo *region_status_changed_infos, uint8_t num_of_changed_regions)
{
    for(uint8_t i = 0; i < num_of_changed_regions; i++){
        set_brightness_of_region(region_status_changed_infos[i].region_id, region_status_changed_infos[i].brightness);
    }
}
*/

static void on_state_changed(DeviceState new_state){
    if (new_state == STATE_TEMPERATURE_ALARM) {
        set_laser_drivers_status(false);
    }
    else if (new_state == STATE_INACTIVITY) {
        if (get_helmet_state()) {
            set_laser_drivers_status(true); //lazeri çalıştırmak demek değil. lazerin çalışabilir durumda olması.
        }
    }
}

static void on_helmet_state_changed(bool helmet_state){
    if (helmet_state) {
        if (get_device_state() == STATE_INACTIVITY) {
            set_laser_drivers_status(true);
        }
    }
    else {
        if (get_device_state() == STATE_ACTIVE) {
            set_laser_drivers_status(false);
        }
    }
}


void initialize_laser_drivers() 
{
    initialize_laser_driver_gpio();
    set_laser_drivers_gpio_pin_status(false);
    set_laser_drivers_status(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    set_banked_leds();
    register_state_change_callback(on_state_changed);
    register_helmet_state_change_callback(on_helmet_state_changed);
}
