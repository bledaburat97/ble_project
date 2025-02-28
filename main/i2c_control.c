#include "i2c_control.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "lp_core_i2c.h"
#include "lp_core_main.h"
#include "lp_core_firmware.h"

#define I2C_MASTER_SCL_IO_1 GPIO_NUM_13    // GPIO number for I2C SCL
#define I2C_MASTER_SDA_IO_1 GPIO_NUM_12    // GPIO number for I2C SDA
#define I2C_MASTER_SCL_IO_2 7   // GPIO number for I2C SCL
#define I2C_MASTER_SDA_IO_2 6    // GPIO number for I2C SDA


static const char *TAG = "I2CControl";
#define I2C_MASTER_FREQ_HZ 400000 // Max I2C frequency for temperature sensors

static void init_i2c_master(uint8_t i2c_num, int sda_io, int scl_io) {
    /*
    ESP_LOGI(TAG, "Init I2C Master");
    gpio_set_direction(sda_io, GPIO_MODE_OUTPUT);
    gpio_set_direction(scl_io, GPIO_MODE_OUTPUT);
    gpio_set_level(sda_io, 1);  // SDA’yı HIGH yap
    gpio_set_level(scl_io, 1);  // SCL’yi HIGH yap
    vTaskDelay(pdMS_TO_TICKS(10));  // **10ms bekleyerek hat temizlensin**
    */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = scl_io,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err;

    err = i2c_param_config(i2c_num, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "I2C parametre yapılandırması başarısız! Hata: 0x%x", err);
        return;
    }

    err = i2c_driver_install(i2c_num, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "I2C sürücüsü yüklenemedi! Hata: 0x%x", err);
    } else {
        ESP_LOGI("I2C", "I2C sürücüsü başarıyla kuruldu.");
    }
}


static void init_lp_i2c_master() {
    ESP_LOGI(TAG, "Initializing LP I2C Master in HP Core...");

    esp_err_t ret = ESP_OK;
    lp_core_i2c_cfg_t i2c_cfg;

    /* Initialize LP I2C with default configuration */
    i2c_cfg.i2c_pin_cfg.sda_io_num = GPIO_NUM_6;
    i2c_cfg.i2c_pin_cfg.scl_io_num = GPIO_NUM_7;
    i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
    i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
    i2c_cfg.i2c_timing_cfg.clk_speed_hz = 100000;
    i2c_cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

    ret = lp_core_i2c_master_init(I2C_SECOND_MASTER_NUM, (const lp_core_i2c_cfg_t*)&i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG,"LP I2C init failed\n");
        abort();
    }
    ESP_LOGI(TAG, "LP I2C initialized successfully\n");

}

void initialize_i2c()
{
    init_i2c_master(I2C_FIRST_MASTER_NUM, I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    //init_lp_i2c_master();
}

esp_err_t write_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number) {
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_address, true);
    i2c_master_write(cmd, data, length, true);

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_master_number, cmd, pdMS_TO_TICKS(10));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", 
                     device_address, reg_address, data[0]);
    } else {
        ESP_LOGE(TAG, "Write failed: Device 0x%02x, Register 0x%02x, Error 0x%x", 
                 device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Write successed"); 

    i2c_cmd_link_delete(cmd);

    return ret; // Return the status of the I2C operation
}


esp_err_t read_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number) {
    if (data == NULL || (length != 1 && length != 2)) {
        ESP_LOGE(TAG, "Invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_address, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_READ, true);

    if (length == 1) {
        i2c_master_read_byte(cmd, &data[0], I2C_MASTER_LAST_NACK);
    } else if (length == 2) {
        i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
        i2c_master_read_byte(cmd, &data[1], I2C_MASTER_LAST_NACK);
    }

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_master_number, cmd, pdMS_TO_TICKS(50));

    if (ret == ESP_OK) {
        if (length == 1) {
            ESP_LOGI(TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", device_address, reg_address, data[0]);
        } else if(length == 2) {
            ESP_LOGI(TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x%02x", device_address, reg_address, data[0], data[1]);
        }
    } else {
        ESP_LOGE(TAG, "Read failed: Device 0x%02x, Register 0x%02x, Error 0x%x", device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t lp_core_send_read_command(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length) {
    ulp_lp_core_device_address = (ulp_lp_core_device_address & 0xFFFFFF00) | (device_address & 0xFF);
    ulp_lp_core_register = (ulp_lp_core_register & 0xFFFFFF00) | (reg_address & 0xFF);
    if(length == 1) {
        ulp_lp_core_command = 1;
    }
    else {
        ulp_lp_core_command = 3;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // LP-Core'un işlemi tamamlamasını bekle
    uint16_t value = ulp_lp_core_value & 0xFFFF;  // Sadece ilk 16 biti al
    if(length == 1) {
        data[0] = value & 0xFF;
    }
    else if (length == 2) {
        data[0] = (value >> 8) & 0xFF;
        data[1] = value & 0xFF;
    }
    ESP_LOGI(TAG, "lp_core_send_read_command: %lu", ulp_lp_core_value);

    return ESP_OK;
}

esp_err_t lp_core_send_write_command(uint8_t device_address, uint8_t reg_address, uint8_t* data, size_t length) {
    ulp_lp_core_device_address = (ulp_lp_core_device_address & 0xFFFFFF00) | (device_address & 0xFF);
    ulp_lp_core_register = (ulp_lp_core_register & 0xFFFFFF00) | (reg_address & 0xFF);
    if(length == 1) {
        ulp_lp_core_value = (ulp_lp_core_value & 0xFFFFFF00) | (data[0] & 0xFF);
    }
    else if(length == 2) {
        ulp_lp_core_value = (ulp_lp_core_value & 0xFFFF0000) | ((data[0] << 8) | data[1]);
    }
    ESP_LOGI(TAG, "lp_core_send_write_command: %lu", ulp_lp_core_value);
    if(length == 1) {
        ulp_lp_core_command = 2; // Yazma komutu gönder
    }
    else {
        ulp_lp_core_command = 4; // Yazma komutu gönder
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // LP-Core'un işlemi tamamlamasını bekle

    return ESP_OK;
}