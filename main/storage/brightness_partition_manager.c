#include "brightness_partition_manager.h"

#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "log_utils.h"

#define BRIGHTNESS_PARTITION_NAME "bright_storage"
#define BRIGHTNESS_PARTITION_SUBTYPE 0x86
#define TAG "BrightnessPartitionManager"
#define BRIGHTNESS_ENTRY_SIZE 7 // 6 byte data + 1 byte crc

static const esp_partition_t* brightness_partition = NULL;

static int max_entry_count(void)
{
    if (!brightness_partition) return 0;
    return (int)(brightness_partition->size / BRIGHTNESS_ENTRY_SIZE);
}

static void encode_entry_bytes(const uint8_t brightness[6], uint8_t out_raw[BRIGHTNESS_ENTRY_SIZE])
{
    memcpy(&out_raw[0], brightness, 6);
    out_raw[6] = calculate_crc8(out_raw, 6);
}

static bool decode_entry_bytes(const uint8_t raw[BRIGHTNESS_ENTRY_SIZE], DefaultBrightnessEntry *out)
{
    if (!raw || !out) return false;

    // boş kontrolü (en azından ilk byte 0xFF ise boş say)
    if (raw[0] == 0xFF) return false;

    uint8_t expected = calculate_crc8(raw, 6);
    if (expected != raw[6]) {
        ESP_LOGW(TAG, "CRC mismatch in brightness entry (expected=0x%02X, got=0x%02X)", expected, raw[6]);
        return false;
    }

    memcpy(out->default_brightness, &raw[0], 6);
    out->crc = raw[6];
    return true;
}

esp_err_t init_brightness_partition() {
    brightness_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, BRIGHTNESS_PARTITION_SUBTYPE, BRIGHTNESS_PARTITION_NAME);

    if (!brightness_partition) {
        ESP_LOGE(TAG, "Brightness partition is not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Brightness partition is successfuly initiated.");

    return ESP_OK;
}

bool default_brightness_read_last(DefaultBrightnessEntry *out) {
    if (!out) return false;
    if (!brightness_partition && init_brightness_partition() != ESP_OK) {
        return false;
    }
    int maxc = max_entry_count();
    DefaultBrightnessEntry last_ok = {0};
    bool any = false;

    for (int i = 0; i < maxc; i++) {
        uint8_t raw[BRIGHTNESS_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(brightness_partition, (size_t)i * BRIGHTNESS_ENTRY_SIZE, raw, BRIGHTNESS_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        // boş entry -> burada bitir
        if (raw[0] == 0xFF) break;

        DefaultBrightnessEntry tmp;
        if (!decode_entry_bytes(raw, &tmp)) {
            // CRC bozuksa burada bitir (append-only log gibi)
            break;
        }

        last_ok = tmp;
        any = true;
    }

    if (!any) return false;
    *out = last_ok;
    return true;
}

bool default_brightness_append(const uint8_t brightness[6])
{
    if (!brightness) return false;
    if (!brightness_partition && init_brightness_partition() != ESP_OK) return false;
    int maxc = max_entry_count();
    
    // (Opsiyonel) aynı entry zaten en sonda varsa tekrar yazma
    DefaultBrightnessEntry last;
    if (default_brightness_read_last(&last)) {
        if (memcmp(last.default_brightness, brightness, 6) == 0) {
            ESP_LOGI(TAG, "default_brightness_append skipped (same as last)");
            return true;
        }
    }

    // ilk boş index'i bul
    int empty_idx = -1;
    for (int i = 0; i < maxc; i++) {
        uint8_t first;
        esp_err_t e = esp_partition_read(brightness_partition, (size_t)i * BRIGHTNESS_ENTRY_SIZE, &first, 1);
        if (e != ESP_OK) return false;
        if (first == 0xFF) {
            empty_idx = i;
            break;
        }
    }

    if (empty_idx < 0) {
        ESP_LOGE(TAG, "Brightness partition is full. Need erase/rollover strategy.");
        return false;
    }

    uint8_t raw[BRIGHTNESS_ENTRY_SIZE];
    encode_entry_bytes(brightness, raw);

    esp_err_t we = esp_partition_write(brightness_partition, (size_t)empty_idx * BRIGHTNESS_ENTRY_SIZE, raw, BRIGHTNESS_ENTRY_SIZE);
    if (we != ESP_OK) {
        ESP_LOGE(TAG, "default_brightness_append write failed: %s", esp_err_to_name(we));
        return false;
    }

    ESP_LOGI(TAG, "default_brightness_append ok idx=%d br={%u,%u,%u,%u,%u,%u}",
             empty_idx, brightness[0], brightness[1], brightness[2], brightness[3], brightness[4], brightness[5]);


    return true;
}