#include <stdio.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include <string.h>
#include "ulp_lp_core_gpio.h"

#define LP_I2C_TRANS_WAIT_FOREVER 1000
#define MAX_BYTE_COUNT 2

volatile uint32_t lp_core_command = NO_COMMAND;
volatile uint32_t lp_core_register = 0;
volatile uint32_t lp_core_value = 0;
volatile uint32_t lp_core_device_address = 0;
volatile uint32_t lp_core_byte_count = 0;

int main(void)
{
    while (1) {
        uint32_t current_lp_command = __atomic_load_n(&lp_core_command, __ATOMIC_RELAXED); // Atomik okuma

        //write
        if(current_lp_command == WRITE_COMMAND) { // <-- Düzeltildi: current_lp_command kullanılıyor
            uint8_t data[MAX_BYTE_COUNT + 1];
            data[0] = __atomic_load_n(&lp_core_register, __ATOMIC_RELAXED) & 0xFF; // Atomik okuma
            for(int i = 1; i <= __atomic_load_n(&lp_core_byte_count, __ATOMIC_RELAXED); i++){ // Atomik okuma
                data[i] = (__atomic_load_n(&lp_core_value, __ATOMIC_RELAXED) >> (8 * (__atomic_load_n(&lp_core_byte_count, __ATOMIC_RELAXED) - i))) & 0xFF; // Atomik okuma
            }
            if(lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, __atomic_load_n(&lp_core_device_address, __ATOMIC_RELAXED), data, __atomic_load_n(&lp_core_byte_count, __ATOMIC_RELAXED) + 1, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) { // Atomik okuma
                __atomic_store_n(&lp_core_command, WRITE_COMPLETED, __ATOMIC_RELAXED); // Atomik yazma
            } else {
                // Hata durumunu ele alın
            }
        }
        //read
        else if(current_lp_command == READ_COMMAND) { // <-- Düzeltildi: current_lp_command kullanılıyor
            uint8_t reg = __atomic_load_n(&lp_core_register, __ATOMIC_RELAXED) & 0xFF; // Atomik okuma
            if(__atomic_load_n(&lp_core_byte_count, __ATOMIC_RELAXED) == 1) { // Atomik okuma
                uint8_t data = 0;
                if(lp_core_i2c_master_write_read_device(LP_I2C_NUM_0, __atomic_load_n(&lp_core_device_address, __ATOMIC_RELAXED), &reg, 1, &data, 1, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) { // Atomik okuma
                    __atomic_store_n(&lp_core_value, (__atomic_load_n(&lp_core_value, __ATOMIC_RELAXED) & 0xFFFFFF00) | (data), __ATOMIC_RELAXED); // Atomik yazma
                    __atomic_store_n(&lp_core_command, READ_COMPLETED, __ATOMIC_RELAXED); // Atomik yazma
                } else {
                    // Hata durumunu ele alın
                }
            }
            else if(__atomic_load_n(&lp_core_byte_count, __ATOMIC_RELAXED) == 2) { // Atomik okuma
                uint8_t data[2] = {0};
                if(lp_core_i2c_master_write_read_device(LP_I2C_NUM_0, __atomic_load_n(&lp_core_device_address, __ATOMIC_RELAXED), &reg, 1, data, 2, LP_I2C_TRANS_WAIT_FOREVER) == ESP_OK) { // Atomik okuma
                    __atomic_store_n(&lp_core_value, ((__atomic_load_n(&lp_core_value, __ATOMIC_RELAXED) & 0xFFFF0000) | (data[0] << 8)) | data[1], __ATOMIC_RELAXED); // Atomik yazma
                    __atomic_store_n(&lp_core_command, READ_COMPLETED, __ATOMIC_RELAXED); // Atomik yazma
                } else {
                    // Hata durumunu ele alın
                }
            }
        }
    }
    return 0;
}