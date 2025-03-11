
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef I2C_CONTROL_H
#define I2C_CONTROL_H

#define I2C_FIRST_MASTER_NUM I2C_NUM_0
#define I2C_SECOND_MASTER_NUM LP_I2C_NUM_0

void init_i2c_master();
void init_lp_i2c_master();
esp_err_t write_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number);
esp_err_t read_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number);

#endif