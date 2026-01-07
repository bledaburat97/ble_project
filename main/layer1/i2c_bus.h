#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    I2C_BUS_MAIN = 0,   // HP core main I2C
    I2C_BUS_LP   = 1,   // LP core I2C (şu an için sadece init, LP komutları ayrı handle)
} I2CBusId;

/**
 * Main I2C bus (HP core) için driver’ı başlatır.
 * device_configuration.h içindeki pin/num değerlerini kullanır.
 */
void i2c_bus_init_main(void);

/**
 * LP core tarafındaki I2C master’ı başlatır.
 * LP-core firmware’i ile konuşacak yapılandırmayı hazırlar.
 */
void i2c_bus_init_lp(void);

/**
 * Main I2C (HP) üzerinden bir register’a yazma.
 * Şu an için sadece I2C_BUS_MAIN destekliyoruz.
 */
esp_err_t i2c_bus_write_reg(uint8_t device_address,
                            uint8_t reg_address,
                            const uint8_t *data,
                            size_t length);

/**
 * Main I2C (HP) üzerinden bir register’dan okuma.
 */
esp_err_t i2c_bus_read_reg(uint8_t device_address,
                           uint8_t reg_address,
                           uint8_t *data,
                           size_t length);

#ifdef __cplusplus
}
#endif
