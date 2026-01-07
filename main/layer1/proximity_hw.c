#include "proximity_hw.h"

#include "i2c_bus.h"
#include "lp_core_queue_manager.h"
#include "proximity_sensor_config.h"  // VCNL_3020_ADDRESS, register adresleri
#include "esp_log.h"

static const char *TAG = "ProximityHW";

static void add_lp_read_command_to_queue(uint8_t device_address,
                                         uint8_t reg_address)
{
    uint32_t dev = (uint32_t)(device_address & 0xFFU);
    uint32_t reg = (uint32_t)(reg_address & 0xFFU);

    lp_core_task_t task = {
        .lp_core_command       = 2U,  // read
        .lp_core_register      = reg,
        .lp_core_value         = 0U,
        .lp_core_device_address= dev,
        .lp_core_byte_count    = 1U
    };

    queue_add_task(task.lp_core_command,
                   task.lp_core_register,
                   task.lp_core_value,
                   task.lp_core_device_address,
                   task.lp_core_byte_count);

    ESP_LOGI(TAG, "LP read queued: dev=0x%02X reg=0x%02X", device_address, reg_address);
}

static void add_lp_write_command_to_queue(uint8_t device_address,
                                          uint8_t reg_address,
                                          uint8_t value)
{
    uint32_t dev = (uint32_t)(device_address & 0xFFU);
    uint32_t reg = (uint32_t)(reg_address & 0xFFU);
    uint32_t val = (uint32_t)(value & 0xFFU);

    lp_core_task_t task = {
        .lp_core_command       = 1U,  // write
        .lp_core_register      = reg,
        .lp_core_value         = val,
        .lp_core_device_address= dev,
        .lp_core_byte_count    = 1U
    };

    queue_add_task(task.lp_core_command,
                   task.lp_core_register,
                   task.lp_core_value,
                   task.lp_core_device_address,
                   task.lp_core_byte_count);

    ESP_LOGI(TAG, "LP write queued: dev=0x%02X reg=0x%02X val=0x%02X",
             device_address, reg_address, value);
}

esp_err_t prox_hw_write_reg(ProximityBus bus,
                            uint8_t device_address,
                            uint8_t reg,
                            uint8_t value)
{
    if (bus == PROX_BUS_HP) {
        return i2c_bus_write_reg(device_address, reg, &value, 1);
    }

    if (bus == PROX_BUS_LP) {
        add_lp_write_command_to_queue(device_address, reg, value);
        return ESP_OK;  // asenkron
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t prox_hw_read_reg_async(ProximityBus bus,
                                 uint8_t device_address,
                                 uint8_t reg)
{
    if (bus == PROX_BUS_HP) {
        uint8_t dummy = 0;
        // HP tarafında sync read isteniyorsa ayrı API kullanmak daha temiz;
        // burada sadece "async" semantiğini koruyoruz.
        esp_err_t err = i2c_bus_read_reg(device_address, reg, &dummy, 1);
        if (err == ESP_OK) {
            // Üst katmana "read completed" durumunu event ile gönderebilirsin.
            prox_hw_on_lp_read_completed(device_address, reg, dummy);
        }
        return err;
    }

    if (bus == PROX_BUS_LP) {
        add_lp_read_command_to_queue(device_address, reg);
        return ESP_OK;  // asenkron
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t prox_hw_read_proximity_result_hp(uint8_t device_address,
                                           uint8_t *high_byte,
                                           uint8_t *low_byte)
{
    if (!high_byte || !low_byte) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = i2c_bus_read_reg(device_address,
                                     PROXIMITY_RESULT_REG_HIGH,
                                     high_byte,
                                     1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PROX_HIGH: dev=0x%02X", device_address);
        return err;
    }

    err = i2c_bus_read_reg(device_address,
                           PROXIMITY_RESULT_REG_LOW,
                           low_byte,
                           1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PROX_LOW: dev=0x%02X", device_address);
        return err;
    }

    return ESP_OK;
}

esp_err_t prox_hw_clear_interrupt(ProximityBus bus,
                                  uint8_t device_address,
                                  uint8_t value)
{
    return prox_hw_write_reg(bus, device_address, INTERRUPT_STATUS_REG, value);
}

/**
 * LP-core tarafında READ_COMPLETED olduğunda çağrılacak helper.
 * Şu an sadece log ve üst seviyeye "hook" bırakıyoruz.
 * Üst seviye (service layer) buradan kendi check_interrupt_status(...)’ünü çağırabilir.
 */
__attribute__((weak))
void prox_hw_on_lp_read_completed(uint8_t device_address,
                                  uint8_t reg,
                                  uint8_t value)
{
    ESP_LOGI(TAG,
             "LP read completed: dev=0x%02X reg=0x%02X val=0x%02X",
             device_address, reg, value);

    // Service layer bu fonksiyonu override edip
    // PROXIMITY_RESULT_REG_HIGH / INTERRUPT_STATUS_REG vs. için
    // kendi iş kuralını uygulayabilir.
}
