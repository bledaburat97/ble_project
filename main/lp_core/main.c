#include <stdio.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include "lp_core_main.h"
#include <string.h>
#include "ulp_lp_core_gpio.h"

#define LP_I2C_TRANS_WAIT_FOREVER  1000

volatile uint32_t lp_core_command = 0;
volatile uint32_t lp_core_register = 0;
volatile uint32_t lp_core_value = 0;
volatile uint32_t lp_core_device_address = 0;
volatile uint32_t lp_core_byte_count = 0;

int main(void)
{
    while (1) {
        //write
        if(lp_core_command == 1) {
            uint8_t data[lp_core_byte_count + 1];
            data[0] = lp_core_register & 0xFF;
            for(int i = 1; i <= lp_core_byte_count; i++){
                data[i] = (lp_core_value >> (8 * (lp_core_byte_count - i))) & 0xFF;
            }
            if(lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, lp_core_device_address, data, lp_core_byte_count + 1, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) {
                lp_core_command = 3;
            }
        }
        //read
        else if(lp_core_command == 2) {
            uint8_t reg = lp_core_register & 0xFF;
            if(lp_core_byte_count == 1) {
                uint8_t data = 0;
                if(lp_core_i2c_master_write_read_device(LP_I2C_NUM_0, lp_core_device_address, &reg, 1, &data, 1, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) {
                    lp_core_value = (lp_core_value & 0xFFFFFF00) | (data);
                    lp_core_command = 4;
                }
            }
            else if(lp_core_byte_count == 2) {
                uint8_t data[2] = {0};
                if(lp_core_i2c_master_write_read_device(LP_I2C_NUM_0, lp_core_device_address, &reg, 1, data, 2, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) {
                    lp_core_value = ((lp_core_value & 0xFFFF0000) | (data[0] << 8)) | data[1];
                    lp_core_command = 4;
                }
            }
        }
    }
    return 0;
}

