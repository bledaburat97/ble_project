#include "default_configuration_handler.h"

#include "../helper/binary_message_parser.h"

#include "../nvs/storage_manager.h"

#include "../ble/include/ble_internal.h"

#include "../device_configuration.h"

#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

static const char *TAG = "DefaultConfigHandler";
static const char *NVS_THERAPY_DUR_KEY = "therapy_dur";
static const char *NVS_BRIGHTNESS_KEY = "bright";

static uint16_t default_therapy_duration = DEFAULT_THERAPY_DURATION;
static uint8_t default_brightness[6] = {100,100,100,100,100,100};

static uint16_t clamp_duration(uint16_t s) {
    if (s < 10) return 10;
    if (s > MAX_THERAPY_DURATION) return MAX_THERAPY_DURATION;
    return s;
}

esp_err_t set_and_store_default_therapy_duration(uint16_t seconds) {
    seconds = clamp_duration(seconds);
    esp_err_t err = save_parameter_u16(NVS_THERAPY_DUR_KEY, seconds);
    if (err == ESP_OK) {
        default_therapy_duration = seconds;
        ESP_LOGI(TAG, "Therapy duration updated to %u", seconds);
    }
    return err;
}

esp_err_t set_and_store_default_brightness(const uint8_t br[6]) {
    // İstersen burada 0–255 aralığı, toplam güç limiti vs. kontrol edebilirsin.
    esp_err_t err = save_parameter_blob(NVS_BRIGHTNESS_KEY, br, 6);
    if (err == ESP_OK) {
        memcpy(default_brightness, br, 6);
        ESP_LOGI(TAG, "Brightness updated to {%u,%u,%u,%u,%u,%u}",
                 br[0], br[1], br[2], br[3], br[4], br[5]);
    }
    return err;
}


static void on_write_of_updating_configuration_message(const uint8_t *buf, size_t len) {
    ActivationMessage activation_message;
    if(!decode_activation_message_bin(buf, &activation_message)) {
        return;
    }

    if(set_and_store_default_therapy_duration(activation_message.duration) != ESP_OK) {
        ESP_LOGW(TAG, "Default duration can not be updated.");
        return;
    }

    if(set_and_store_default_brightness(activation_message.brightness) != ESP_OK) {
        ESP_LOGW(TAG, "Default brightness can not be updated.");
    }

}

void init_default_configuration_handler(void) {
    uint16_t dur = 0;
    if (read_parameter_u16(NVS_THERAPY_DUR_KEY, &dur) != ESP_OK) {
        dur = default_therapy_duration;
        (void)save_parameter_u16(NVS_THERAPY_DUR_KEY, dur);
    }
    default_therapy_duration = dur;
    uint8_t brightness[6];
    size_t  blen = sizeof(brightness);
    if (read_parameter_blob(NVS_BRIGHTNESS_KEY, brightness, &blen) != ESP_OK || blen != 6) {
        (void)save_parameter_blob(NVS_BRIGHTNESS_KEY, default_brightness, sizeof(default_brightness));
        memcpy(brightness, default_brightness, 6);
    }
    memcpy(default_brightness, brightness, 6);
    register_on_write_updating_configuration_callback(on_write_of_updating_configuration_message);
}

uint16_t get_default_therapy_duration(void) {
    return default_therapy_duration;
}

const uint8_t* get_default_brightness(void) {
    return default_brightness;
}

