#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROX_BUS_HP = 0,   // HP core, main I2C
    PROX_BUS_LP = 1,   // LP core, LP I2C + ulp komut
} ProximityBus;

/**
 * Proximity sensörünün COMMON register’larına yazmak için generic API.
 */
esp_err_t prox_hw_write_reg(ProximityBus bus,
                            uint8_t device_address,
                            uint8_t reg,
                            uint8_t value);

/**
 * Proximity sensörünün COMMON register’larından okumak için generic API.
 * HP için direkt I2C, LP için LP-core read queue kullanır.
 *
 * LP tarafında bu fonksiyon asenkron olabilir; şimdilik:
 * - HP için: out_value hemen doldurulur
 * - LP için: fonksiyon sadece "read isteğini" queue’ya ekler, 
 *   gerçek değer LP-core’dan geldiğinde üst katmana event ile geçilir.
 */
esp_err_t prox_hw_read_reg_async(ProximityBus bus,
                                 uint8_t device_address,
                                 uint8_t reg);

/**
 * LP-core’dan gelen "read completed" callback’inde kullanılacak yardımcı:
 * LP-core shared memory’deki value’yu üst katmana taşımak için kullanılır.
 * (Senin mevcut `check_interrupt_status(...)`’e giden akış burada çağrılacak.)
 */
void prox_hw_on_lp_read_completed(uint8_t device_address,
                                  uint8_t reg,
                                  uint8_t value);

/**
 * HP tarafında anlık proximity sonucu okumak için sync helper.
 */
esp_err_t prox_hw_read_proximity_result_hp(uint8_t device_address,
                                           uint8_t *high_byte,
                                           uint8_t *low_byte);

/**
 * HP/LP için interrupt status register’ına yazma (clear).
 */
esp_err_t prox_hw_clear_interrupt(ProximityBus bus,
                                  uint8_t device_address,
                                  uint8_t value);

#ifdef __cplusplus
}
#endif
