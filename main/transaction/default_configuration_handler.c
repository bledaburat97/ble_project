#include "default_configuration_handler.h"

#include "../helper/binary_message_parser.h"

#include "../ble/include/ble_internal.h"

#include "../device_configuration.h"

#include "../storage/brightness_partition_manager.h"
#include "../storage/duration_partition_manager.h"

#include "esp_log.h"

static const char *TAG = "DefaultConfigHandler";

// Varsayılan terapi süre/parlaklık değerleri (RAM'de cache).
static uint16_t default_therapy_duration = DEFAULT_THERAPY_DURATION;
static uint8_t default_brightness[6] = {100,100,100,100,100,100};

// Terapi süresini güvenli aralığa sınırlar.


// Varsayılan terapi süresini flasha yazar.
esp_err_t set_and_store_default_therapy_duration(const uint16_t seconds) {

    if (default_duration_append(seconds)) {
        // clamp’lenmiş değeri RAM’e tam yansıtmak için en garanti yöntem olarak tekrar oku
        DurationEntry last;
        if (default_duration_read_last(&last)) {
            default_therapy_duration = last.duration;
        } else {
            // fallback
            default_therapy_duration = seconds;
        }
        ESP_LOGI(TAG, "Therapy duration updated to %u", default_therapy_duration);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Varsayılan parlaklığı flasha yazar.
esp_err_t set_and_store_default_brightness(const uint8_t brightness[6]) {
    if (!brightness) return ESP_ERR_INVALID_ARG;

    if (default_brightness_append(brightness)) {
        memcpy(default_brightness, brightness, 6);
        ESP_LOGI(TAG, "Brightness updated to {%u,%u,%u,%u,%u,%u}",
                 brightness[0], brightness[1], brightness[2], brightness[3], brightness[4], brightness[5]);
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Uygulamadan gelen "varsayılan config" mesajını işler.
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

// Partitionlardan varsayılanları yükler ve write callback'ini bağlar.
void init_default_configuration_handler(void) {

    init_default_duration_partition();
    DurationEntry last_duration;
    if (!default_duration_read_last(&last_duration)) {
        // hiç kayıt yok -> default'u yaz
        default_duration_append(default_therapy_duration);
    } else {
        default_therapy_duration = last_duration.duration;
    }


    init_brightness_partition();

    DefaultBrightnessEntry last_brightness;
    if (!default_brightness_read_last(&last_brightness)) {
        // hiç kayıt yok -> default'u yaz
        (void)default_brightness_append(default_brightness);
    } else {
        memcpy(default_brightness, last_brightness.default_brightness, 6);
    }

    register_on_write_updating_configuration_callback(on_write_of_updating_configuration_message);
}

// Varsayılan terapi süresini döner.
uint16_t get_default_therapy_duration(void) {
    return default_therapy_duration;
}

// Varsayılan parlaklık listesini döner.
const uint8_t* get_default_brightness(void) {
    return default_brightness;
}

