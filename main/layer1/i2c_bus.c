#include "i2c_bus.h"

#include "device_configuration.h"
#include "lp_core_i2c.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MAIN_FREQ_HZ    400000
#define I2C_LP_FREQ_HZ      100000

static const char *TAG = "I2CBus";

void i2c_bus_init_main(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO_1,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MAIN_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_FIRST_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed: 0x%x", err);
        return;
    }

    err = i2c_driver_install(I2C_FIRST_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: 0x%x", err);
    } else {
        ESP_LOGI(TAG, "I2C main bus initialized.");
    }
}

void i2c_bus_init_lp(void)
{
    ESP_LOGI(TAG, "Initializing LP I2C bus...");

    lp_core_i2c_cfg_t cfg = { 0 };
    cfg.i2c_pin_cfg.sda_io_num    = I2C_MASTER_SDA_IO_2;
    cfg.i2c_pin_cfg.scl_io_num    = I2C_MASTER_SCL_IO_2;
    cfg.i2c_pin_cfg.sda_pullup_en = true;
    cfg.i2c_pin_cfg.scl_pullup_en = true;
    cfg.i2c_timing_cfg.clk_speed_hz = I2C_LP_FREQ_HZ;
    cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

    esp_err_t ret = lp_core_i2c_master_init(I2C_SECOND_MASTER_NUM, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP I2C init failed: 0x%x", ret);
        abort();
    }

    ESP_LOGI(TAG, "LP I2C initialized successfully.");
}

esp_err_t i2c_bus_write_reg(uint8_t device_address,
                            uint8_t reg_address,
                            const uint8_t *data,
                            size_t length)
{
    if (data == NULL || length == 0) {
        ESP_LOGE(TAG, "Invalid data pointer or length in write_reg");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = i2c_master_start(cmd);
    if (ret == ESP_OK) {
        ret = i2c_master_write_byte(cmd,
                                    (device_address << 1) | I2C_MASTER_WRITE,
                                    true);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_write_byte(cmd, reg_address, true);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_write(cmd, (uint8_t *)data, length, true);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_stop(cmd);
    }

    if (ret == ESP_OK) {
        ret = i2c_master_cmd_begin(I2C_FIRST_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: dev=0x%02X reg=0x%02X err=0x%x",
                 device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t i2c_bus_read_reg(uint8_t device_address,
                           uint8_t reg_address,
                           uint8_t *data,
                           size_t length)
{
    if (data == NULL || length == 0) {
        ESP_LOGE(TAG, "Invalid data pointer or length in read_reg");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = i2c_master_start(cmd);
    if (ret == ESP_OK) {
        ret = i2c_master_write_byte(cmd,
                                    (device_address << 1) | I2C_MASTER_WRITE,
                                    true);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_write_byte(cmd, reg_address, true);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_start(cmd);
    }
    if (ret == ESP_OK) {
        ret = i2c_master_write_byte(cmd,
                                    (device_address << 1) | I2C_MASTER_READ,
                                    true);
    }

    if (ret == ESP_OK) {
        if (length == 1) {
            ret = i2c_master_read_byte(cmd, &data[0], I2C_MASTER_LAST_NACK);
        } else {
            ret = i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
            if (ret == ESP_OK) {
                ret = i2c_master_read_byte(cmd,
                                           &data[length - 1],
                                           I2C_MASTER_LAST_NACK);
            }
        }
    }

    if (ret == ESP_OK) {
        ret = i2c_master_stop(cmd);
    }

    if (ret == ESP_OK) {
        ret = i2c_master_cmd_begin(I2C_FIRST_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: dev=0x%02X reg=0x%02X err=0x%x",
                 device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    i2c_cmd_link_delete(cmd);
    return ret;
}
