#include "i2c_control.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "lp_core_i2c.h"
#include "lp_core_main.h"
#include "lp_core_firmware.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_SCL_IO_1 GPIO_NUM_14
#define I2C_MASTER_SDA_IO_1 GPIO_NUM_15
#define I2C_MASTER_SCL_IO_2 GPIO_NUM_7
#define I2C_MASTER_SDA_IO_2 GPIO_NUM_6

static const char *TAG = "I2CControl";
#define I2C_MASTER_FREQ_HZ 400000
#define LP_I2C_MASTER_FREQ_HZ 100000

/**
 * @brief I2C Bus tarayıcı görevi.
 * Bulunan I2C cihaz adreslerini seri monitöre yazdırır.
 */
void i2c_scanner_task(void *arg)
{
    ESP_LOGI(TAG, "I2C Tarama Başlatılıyor...");
    while (1) {
        printf("I2C Bus Scan:\n");
        printf("   0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
        printf("00:   ");
        for (int i = 0; i < 0x78; i++) { // I2C adresleri 0x00 ile 0x7F arasındadır, 0x78 son yaygın adrestir.
            if (i % 16 == 0 && i != 0) {
                printf("\n%02x:", i);
            }
            // Özel adresleri atla (örn. genel çağrı adresleri)
            if (i == 0 || i == 0x78 || i == 0x79) { // 0x78 ve 0x79, 10-bit adresleme için ayrılmıştır.
                printf(" --");
                continue;
            }

            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true); // Yazma denemesi
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_FIRST_MASTER_NUM, cmd, pdMS_TO_TICKS(10)); // Kısa timeout

            if (ret == ESP_OK) {
                printf(" %02x", i); // Cihaz yanıt verdi!
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf(" --"); // Cihaz yanıt vermedi (timeout)
            } else {
                printf(" XX"); // Diğer hatalar
            }
            i2c_cmd_link_delete(cmd);
            vTaskDelay(pdMS_TO_TICKS(10)); // Bir sonraki denemeden önce kısa bir gecikme
        }
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(20000)); // Her 5 saniyede bir tarama yap
    }
}

void init_i2c_master() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO_1,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err;

    err = i2c_param_config(I2C_FIRST_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "I2C parametre yapılandırması başarısız! Hata: 0x%x", err);
        return;
    }

    err = i2c_driver_install(I2C_FIRST_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "I2C sürücüsü yüklenemedi! Hata: 0x%x", err);
    } else {
        ESP_LOGI("I2C", "I2C sürücüsü başarıyla kuruldu.");
    }
}

void init_lp_i2c_master() {
    ESP_LOGI(TAG, "Initializing LP I2C Master in HP Core...");

    esp_err_t ret = ESP_OK;
    lp_core_i2c_cfg_t i2c_cfg;

    i2c_cfg.i2c_pin_cfg.sda_io_num = I2C_MASTER_SDA_IO_2;
    i2c_cfg.i2c_pin_cfg.scl_io_num = I2C_MASTER_SCL_IO_2;
    i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
    i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
    i2c_cfg.i2c_timing_cfg.clk_speed_hz = LP_I2C_MASTER_FREQ_HZ;
    i2c_cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

    ret = lp_core_i2c_master_init(I2C_SECOND_MASTER_NUM, (const lp_core_i2c_cfg_t*)&i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG,"LP I2C init failed\n");
        abort();
    }
    ESP_LOGI(TAG, "LP I2C initialized successfully\n");

}

esp_err_t write_register(uint8_t device_address, uint8_t reg_address, uint8_t *data, size_t length, uint8_t i2c_master_number) {
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data pointer or length");
        return ESP_ERR_INVALID_ARG;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t ret = i2c_master_start(cmd);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Start successful:");
    }
    ret = i2c_master_write_byte(cmd, (device_address << 1) | I2C_MASTER_WRITE, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write device address successful:");
    }
    ret = i2c_master_write_byte(cmd, reg_address, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write reg address successful:");
    }
    ret = i2c_master_write(cmd, data, length, true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write data successful:");
    }
    ret = i2c_master_stop(cmd);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Stop successful:");
    }
    ret = i2c_master_cmd_begin(i2c_master_number, cmd, pdMS_TO_TICKS(10));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", 
                     device_address, reg_address, data[0]);
    } else {
        ESP_LOGE(TAG, "Write failed: Device 0x%02x, Register 0x%02x, Error 0x%x", 
                 device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    i2c_cmd_link_delete(cmd);

    return ret;
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
            //ESP_LOGI(TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x", device_address, reg_address, data[0]);
        } else if(length == 2) {
            //ESP_LOGI(TAG, "Read successful: Device 0x%02x, Register 0x%02x, Data 0x%02x%02x", device_address, reg_address, data[0], data[1]);
        }
    } else {
        ESP_LOGE(TAG, "Read failed: Device 0x%02x, Register 0x%02x, Error 0x%x", device_address, reg_address, ret);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    i2c_cmd_link_delete(cmd);
    return ret;
}