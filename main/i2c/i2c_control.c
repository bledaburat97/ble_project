#include "i2c_control.h"

#include "../lp_core/lp_core_main.h"
#include "../device_configuration.h"

#include "esp_log.h"
#include "lp_core_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_FREQ_HZ     400000
#define LP_I2C_MASTER_FREQ_HZ  100000

static const char *TAG = "I2CControl";
static SemaphoreHandle_t s_i2c_mutex = NULL;

static void ensure_i2c_mutex(void)
{
    if (s_i2c_mutex == NULL) {
        s_i2c_mutex = xSemaphoreCreateMutex();
        if (s_i2c_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create I2C mutex!");
        }
    }
}

static bool is_i2c_timeout(esp_err_t err)
{
    return err == ESP_ERR_TIMEOUT;
}

void init_i2c_master(void)
{
    ensure_i2c_mutex();

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO_1,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_FIRST_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed! Error: 0x%x", err);
        return;
    }

    err = i2c_driver_install(I2C_FIRST_MASTER_NUM, conf.mode, 0, 0, 0);

    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "I2C driver already installed (port=%d).", I2C_FIRST_MASTER_NUM);
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver installation failed. Error: 0x%x", err);
    } else {
        ESP_LOGI(TAG, "I2C driver successfully installed.");
    }
}

void init_lp_i2c_master(void)
{
    ESP_LOGI(TAG, "Initializing LP I2C Master in HP core...");

    lp_core_i2c_cfg_t i2c_cfg;

    i2c_cfg.i2c_pin_cfg.sda_io_num     = I2C_MASTER_SDA_IO_2;
    i2c_cfg.i2c_pin_cfg.scl_io_num     = I2C_MASTER_SCL_IO_2;
    i2c_cfg.i2c_pin_cfg.sda_pullup_en  = true;
    i2c_cfg.i2c_pin_cfg.scl_pullup_en  = true;
    i2c_cfg.i2c_timing_cfg.clk_speed_hz = LP_I2C_MASTER_FREQ_HZ;
    i2c_cfg.i2c_src_clk                = LP_I2C_SCLK_LP_FAST;

    esp_err_t ret = lp_core_i2c_master_init(I2C_SECOND_MASTER_NUM, (const lp_core_i2c_cfg_t *)&i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP I2C init failed (err=0x%x)", ret);
        abort();
    }

    ESP_LOGI(TAG, "LP I2C initialized successfully.");
}

static esp_err_t i2c_cmd_begin_with_retry(i2c_cmd_handle_t cmd, TickType_t timeout_ticks)
{
    esp_err_t ret = i2c_master_cmd_begin(I2C_FIRST_MASTER_NUM, cmd, timeout_ticks);
    if (ret == ESP_OK) return ret;

    if (is_i2c_timeout(ret)) {
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = i2c_master_cmd_begin(I2C_FIRST_MASTER_NUM, cmd, timeout_ticks);
    }
    return ret;
}

esp_err_t write_register(uint8_t device_address,
                         uint8_t reg_address,
                         uint8_t *data,
                         size_t length)
{
    if (data == NULL || length == 0) {
        ESP_LOGE(TAG, "write_register: invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

     ensure_i2c_mutex();
    if (s_i2c_mutex == NULL) return ESP_ERR_NO_MEM;

    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout (write).");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        xSemaphoreGive(s_i2c_mutex);
        return ESP_ERR_NO_MEM;
    }

    do {
        ret = i2c_master_start(cmd);
        if (ret != ESP_OK) break;

        ret = i2c_master_write_byte(cmd, (uint8_t)((device_address << 1) | I2C_MASTER_WRITE), true);
        if (ret != ESP_OK) break;

        ret = i2c_master_write_byte(cmd, reg_address, true);
        if (ret != ESP_OK) break;

        ret = i2c_master_write(cmd, data, length, true);
        if (ret != ESP_OK) break;

        ret = i2c_master_stop(cmd);
        if (ret != ESP_OK) break;

        ret = i2c_cmd_begin_with_retry(cmd, pdMS_TO_TICKS(50));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C write failed: dev=0x%02X reg=0x%02X err=0x%x",
                     device_address, reg_address, ret);
        }
    } while (0);

    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(s_i2c_mutex);

    return ret;
}

esp_err_t read_register(uint8_t device_address,
                        uint8_t reg_address,
                        uint8_t *data,
                        size_t length)
{
    if (data == NULL || length == 0) {
        ESP_LOGE(TAG, "read_register: invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    ensure_i2c_mutex();
    if (s_i2c_mutex == NULL) return ESP_ERR_NO_MEM;

    if (xSemaphoreTake(s_i2c_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        ESP_LOGE(TAG, "I2C mutex timeout (read).");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_FAIL;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        xSemaphoreGive(s_i2c_mutex);
        return ESP_ERR_NO_MEM;
    }

    do {
        ret = i2c_master_start(cmd);
        if (ret != ESP_OK) break;

        ret = i2c_master_write_byte(cmd, (uint8_t)((device_address << 1) | I2C_MASTER_WRITE), true);
        if (ret != ESP_OK) break;

        ret = i2c_master_write_byte(cmd, reg_address, true);
        if (ret != ESP_OK) break;

        ret = i2c_master_start(cmd);
        if (ret != ESP_OK) break;

        ret = i2c_master_write_byte(cmd, (uint8_t)((device_address << 1) | I2C_MASTER_READ), true);
        if (ret != ESP_OK) break;

        if (length == 1) {
            ret = i2c_master_read_byte(cmd, &data[0], I2C_MASTER_LAST_NACK);
            if (ret != ESP_OK) break;
        } else {
            ret = i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
            if (ret != ESP_OK) break;

            ret = i2c_master_read_byte(cmd, &data[length - 1], I2C_MASTER_LAST_NACK);
            if (ret != ESP_OK) break;
        }

        ret = i2c_master_stop(cmd);
        if (ret != ESP_OK) break;

        ret = i2c_cmd_begin_with_retry(cmd, pdMS_TO_TICKS(80));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C read failed: dev=0x%02X reg=0x%02X err=0x%x",
                     device_address, reg_address, ret);
        }
    } while (0);

    i2c_cmd_link_delete(cmd);
    xSemaphoreGive(s_i2c_mutex);

    return ret;
}
