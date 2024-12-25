#include "laser_driver_control.h"
#include "i2c_control.h"
#include "ble_control.h"
#include "esp_log.h"
#include "string.h"


#define FIRST_LED_DRIVER_BANKED_COUNT 24
#define SECOND_LED_DRIVER_BANKED_COUNT 24

#define LP5036_ADDRESS_1 0x30   // I2C address for the first LP5036
#define LP5036_ADDRESS_2 0x31   // I2C address for the second LP5036
#define LED_CONFIG0_REG 0x02
#define LED_CONFIG1_REG 0x03
#define BANK_BRIGHTNESS_REG 0x04
#define BANK_A_COLOR_REG 0x05
#define OUT0_COLOR_REG 0x14

#define NUM_OF_LP5036 2
static const char *LASER_TAG = "LaserDriverControl";
bool is_notification_on = true;

static const LP5036Info lp5036Infos[NUM_OF_LP5036] = {
    {
        .address = LP5036_ADDRESS_1,
        .region = {
            {
                .regionId = 1,
                .numOfLEDs = 12,
                .ledList = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                .isBank = true
            },
            {
                .regionId = 2,
                .numOfLEDs = 0,
                .ledList = {0},  // Empty list since there are no LEDs in this region
                .isBank = false
            },
            {
                .regionId = 3,
                .numOfLEDs = 14,
                .ledList = {13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26},
                .isBank = false
            },
            {
                .regionId = 4,
                .numOfLEDs = 10,
                .ledList = {27, 28, 29, 30, 31, 32, 33, 34, 35, 36},
                .isBank = false
            },
            {
                .regionId = 5,
                .numOfLEDs = 0,
                .ledList = {0},
                .isBank = false
            }
        }
    },
    {
        .address = LP5036_ADDRESS_2,
        .region = {
            {
                .regionId = 1,
                .numOfLEDs = 0,
                .ledList = {0},
                .isBank = false
            },
            {
                .regionId = 2,
                .numOfLEDs = 24,
                .ledList = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
                .isBank = true
            },
            {
                .regionId = 3,
                .numOfLEDs = 0,
                .ledList = {0},
                .isBank = false
            },
            {
                .regionId = 4,
                .numOfLEDs = 4,
                .ledList = {25, 26, 27, 28},
                .isBank = false
            },
            {
                .regionId = 5,
                .numOfLEDs = 8,
                .ledList = {29, 30, 31, 32, 33, 34, 35, 36},
                .isBank = false
            }
        }
    }
};

static void checkRegions()
{
    for(int i = 0; i < NUM_OF_LP5036; i++)
    {
        int totalNumOfLeds = 0;
        for (int j = 0; j < TOTAL_REGION_COUNT; j++)
        {
            totalNumOfLeds += lp5036Infos[i].region[j].numOfLEDs;
        }

        if (totalNumOfLeds > MAX_NUM_OF_LED_OF_LP5036)
        {
            ESP_LOGE(LASER_TAG, "Input total number of leds exceeds limit: %d for the LP5036 %d", totalNumOfLeds, i);
            return;
        }
    }
}

static void set_banked_leds()
{
    for(int i = 0; i < NUM_OF_LP5036; i++)
    {

        for(int j = 0; j < TOTAL_REGION_COUNT; j++)
        {
            if(lp5036Infos[i].region[j].isBank)
            {
                uint8_t result = lp5036Infos[i].region[j].numOfLEDs / 3; 
                uint16_t led_config = (1 << result) - 1;

                uint8_t led_config0_data = led_config & 0xFF;
                uint8_t led_config1_data = (led_config >> 8) & 0xFF;
                ESP_LOGI(LASER_TAG, "LED_CONFIG0: 0x%02x, LED_CONFIG1: 0x%02x", led_config0_data, led_config1_data);
                write_register(lp5036Infos[i].address, LED_CONFIG0_REG, &led_config0_data, 1, I2C_FIRST_MASTER_NUM);
                write_register(lp5036Infos[i].address, LED_CONFIG1_REG, &led_config1_data, 1, I2C_FIRST_MASTER_NUM);
                break;
            }
        }
        
        uint8_t default_color = 0xFF;

        write_register(lp5036Infos[i].address, BANK_A_COLOR_REG, &default_color, 1, I2C_FIRST_MASTER_NUM);
        
        uint8_t default_brightness = 0x00;
        write_register(lp5036Infos[i].address, BANK_BRIGHTNESS_REG, &default_brightness, 1, I2C_FIRST_MASTER_NUM);
    }
}

static void set_brightness_of_region(int regionId, uint8_t brightness){
    for(int i = 0; i < NUM_OF_LP5036; i++)
    {
        if(lp5036Infos[i].region[regionId - 1].isBank)
        {
            write_register(lp5036Infos[i].address, BANK_BRIGHTNESS_REG, &brightness, 1, I2C_FIRST_MASTER_NUM);
        }
        else
        {
            for (int j = 0; j < lp5036Infos[i].region[regionId - 1].numOfLEDs; j++){
                write_register(lp5036Infos[i].address, OUT0_COLOR_REG + lp5036Infos[i].region[regionId - 1].ledList[j], &brightness, 1, I2C_FIRST_MASTER_NUM);
            }
        }
    }
}


static uint8_t read_led_brightness(uint8_t LP5036_address_id, uint8_t led_index, uint8_t banked_led_count) {
    uint8_t result;
    if (led_index >= banked_led_count) 
    {
        read_register(LP5036_address_id, OUT0_COLOR_REG + led_index, &result, 1, I2C_FIRST_MASTER_NUM);
    } else {
        read_register(LP5036_address_id, BANK_BRIGHTNESS_REG, &result, 1, I2C_FIRST_MASTER_NUM);
    }
    return result;
}

static uint16_t count_active_leds() {
    uint16_t count = 0;
    // Logic to count the active LEDs, e.g., based on brightness values or registers

    // Assuming you have functions that can read the status or brightness of each LED
    for (uint8_t i = 0; i < MAX_NUM_OF_LED_OF_LP5036; i++) {
        uint8_t brightness = read_led_brightness(LP5036_ADDRESS_1, i, FIRST_LED_DRIVER_BANKED_COUNT);
        if (brightness > 0) {
            count++;
        }
    }

    for (uint8_t i = 0; i < MAX_NUM_OF_LED_OF_LP5036; i++) {
        uint8_t brightness = read_led_brightness(LP5036_ADDRESS_2, i, SECOND_LED_DRIVER_BANKED_COUNT);
        if (brightness > 0) {
            count++;
        }
    }

    return count;
}

void set_brightness(RegionStatusChangedInfo *region_status_changed_infos, int num_of_changed_regions)
{
    for(int i = 0; i < num_of_changed_regions; i++){
        if(region_status_changed_infos[i].on)
        {
            set_brightness_of_region(region_status_changed_infos[i].region_id, region_status_changed_infos[i].brightness); 
        }
        else
        {
            set_brightness_of_region(region_status_changed_infos[i].region_id, 0x00); 
        }
    }
}

void initialize_laser_driver() 
{
    uint8_t region_1_default_brightness = 0xFF;
    uint8_t region_2_default_brightness = 0xFF;
    uint8_t region_3_default_brightness = 0xFF;
    uint8_t region_4_default_brightness = 0xFF;
    uint8_t region_5_default_brightness = 0xFF;

    checkRegions();
    set_banked_leds();
    set_brightness_of_region(1, region_1_default_brightness);
    set_brightness_of_region(2, region_2_default_brightness);
    set_brightness_of_region(3, region_3_default_brightness);
    set_brightness_of_region(4, region_4_default_brightness);
    set_brightness_of_region(5, region_5_default_brightness);
}

void setDataOfActiveLaserCount(uint8_t* data) {
    uint16_t active_leds = count_active_leds();
    data[0] = active_leds & 0xFF;
    data[1] = (active_leds >> 8) & 0xFF;
} 

void stop_notification() {
    is_notification_on = false;
}