#include <stdio.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include "lp_core_main.h"
#include <string.h>
#include "ulp_lp_core_gpio.h"

#define I2C_SECOND_MASTER_NUM LP_I2C_NUM_0
#define LP_I2C_TRANS_WAIT_FOREVER   100
#define WAKEUP_PIN LP_IO_NUM_0

/* this variable will be exported as a public symbol, visible from main CPU: */
bool gpio_level_previous = false;


volatile uint32_t lp_core_command = 0;
volatile uint32_t lp_core_register = 0;
volatile uint32_t lp_core_value = 0;
volatile uint32_t lp_core_device_address = 0;
volatile uint32_t lp_core_byte_count = 0;
volatile uint32_t lp_core_result = 0;

esp_err_t lp_core_i2c_write(uint16_t device_address, uint8_t reg_address, uint8_t *data, size_t length) {
    uint8_t buffer[length + 1];
    buffer[0] = reg_address;
    for (size_t i = 0; i < length; i++) {
        buffer[i + 1] = data[i];
    }

    esp_err_t ret = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, device_address, buffer, length + 1, LP_I2C_TRANS_WAIT_FOREVER);

    return ret;
}


int main(void)
{
    lp_core_value = 0;
    while (1) {
        if (lp_core_command == 1 || lp_core_command == 3) {
            lp_core_value = 0;
            lp_core_byte_count = 1;
            if (lp_core_command == 3) {
                lp_core_byte_count = 2;
            }
            uint8_t data[2] = {0};
            lp_core_result = 8;
            uint16_t device_address = lp_core_device_address & 0xFFFF;
            uint8_t reg_address = lp_core_register & 0xFF;
            lp_core_result = 1;
            if (lp_core_byte_count != 1 && lp_core_byte_count != 2) {
                lp_core_result = 1;
                //continue;
            }
        
            if(lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, device_address, &reg_address, 1, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) {
                lp_core_result = 2;
            }
            else{
                lp_core_result = 3;
                //continue;
            }

            if(lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, device_address, data, lp_core_byte_count, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) {
                lp_core_result = 4;
            }
            else{
                lp_core_result = 5;
                //continue;
            }
            if (lp_core_byte_count == 1) {
                lp_core_value = (lp_core_value & 0xFFFFFF00) | (data[0] & 0xFF);
            } else if(lp_core_byte_count == 2){
                lp_core_value = (lp_core_value & 0xFFFF0000) | ((data[0] << 8) | data[1]);
            }
            lp_core_result = 6;
        } else if (lp_core_command == 2 || lp_core_command == 4) {
            uint8_t lp_core_byte_count = 1;
            if (lp_core_command == 4) {
                lp_core_byte_count = 2;
            }
            uint16_t device_address = lp_core_device_address & 0xFFFF;
            uint8_t reg_address = lp_core_register & 0xFF;
            if (lp_core_byte_count != 1 && lp_core_byte_count != 2) {
                lp_core_result = 7;
                //continue;
            }

            if (lp_core_byte_count == 1) {
                uint8_t data = lp_core_value & 0xFF;
                if(lp_core_i2c_write(device_address, reg_address, &data, 1) == ESP_OK ){
                    lp_core_result = 8;
                }
                else{
                    lp_core_result = 9;
                    //continue;
                }
            } else if(lp_core_byte_count == 2){
                uint8_t data[2] = {(lp_core_value >> 8) & 0xFF , lp_core_value & 0xFF};
                if(lp_core_i2c_write(device_address, reg_address, data, 2) == ESP_OK ){
                    lp_core_result = 10;
                }
                else{
                    lp_core_result = 11;
                    //continue;
                }
            }
        }

        //ulp_lp_core_wakeup_main_processor();
    }
    return 0;
    
}

