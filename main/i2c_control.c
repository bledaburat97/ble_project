#include "i2c_control.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO_1 23    // GPIO number for I2C SCL
#define I2C_MASTER_SDA_IO_1 22    // GPIO number for I2C SDA
#define I2C_MASTER_SCL_IO_2 7   // GPIO number for I2C SCL
#define I2C_MASTER_SDA_IO_2 6    // GPIO number for I2C SDA


static const char *I2C_TAG = "I2CControl";
#define I2C_MASTER_FREQ_HZ 400000 // Max I2C frequency for temperature sensors

static void init_i2c_master(uint8_t i2c_num, int sda_io, int scl_io) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_io,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(i2c_num, &conf);
    i2c_driver_install(i2c_num, conf.mode, 0, 0, 0);
}

void initialize_i2c()
{
    init_i2c_master(I2C_FIRST_MASTER_NUM, I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    init_i2c_master(I2C_SECOND_MASTER_NUM, I2C_MASTER_SDA_IO_2, I2C_MASTER_SCL_IO_2);
}


esp_err_t write_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number) {
    if (data == NULL) {
        ESP_LOGE(I2C_TAG, "Invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_address, true);
    i2c_master_write(cmd, data, length, true);

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_master_number, cmd, pdMS_TO_TICKS(1000));
    if (ret == ESP_OK) {
        ESP_LOGI(I2C_TAG, "Write successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", 
                     device_address, reg_address, data[0]);
    } else {
        ESP_LOGE(I2C_TAG, "Write failed: Device 0x%02x, Register 0x%02x, Error 0x%x", 
                 device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    i2c_cmd_link_delete(cmd);

    return ret; // Return the status of the I2C operation
}


esp_err_t read_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number) {
    if (data == NULL || (length != 1 && length != 2)) {
        ESP_LOGE(I2C_TAG, "Invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_address, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_READ, true);

    if (length == 1) {
        i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    } else if (length == 2) {
        i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
    }

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_master_number, cmd, pdMS_TO_TICKS(1000));
    if (ret == ESP_OK) {
        if (length == 1) {
            ESP_LOGI(I2C_TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", device_address, reg_address, data[0]);
        } else if(length == 2) {
            ESP_LOGI(I2C_TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x%02x", device_address, reg_address, data[0], data[1]);
        }
    } else {
        ESP_LOGE(I2C_TAG, "Read failed: Device 0x%02x, Register 0x%02x, Error 0x%x", device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    i2c_cmd_link_delete(cmd);
    return ret;
}