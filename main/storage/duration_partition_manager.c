#include "duration_partition_manager.h"

#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "log_utils.h"

#include "../device_configuration.h"

#define DURATION_PARTITION_NAME "duration_storage"
#define DURATION_PARTITION_SUBTYPE 0x87
#define TAG "DurationPartitionManager"
#define DURATION_ENTRY_SIZE 3

static const esp_partition_t* duration_partition = NULL;

static inline void wr_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static int max_entry_count(void)
{
    if (!duration_partition) return 0;
    return (int)(duration_partition->size / DURATION_ENTRY_SIZE);
}

static uint16_t clamp_duration(uint16_t s) {
    if (s < 10) return 10;
    if (s > MAX_THERAPY_DURATION) return MAX_THERAPY_DURATION;
    return s;
}

static void encode_entry_bytes(uint16_t duration, uint8_t out_raw[DURATION_ENTRY_SIZE])
{
    memset(out_raw, 0xFF, DURATION_ENTRY_SIZE);
    wr_le16(&out_raw[0], duration);
    out_raw[2] = calculate_crc8(out_raw, 2);
}

static inline uint16_t le16(const uint8_t *p)
{
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

static bool decode_entry_bytes(const uint8_t raw[DURATION_ENTRY_SIZE], DurationEntry *out)
{
    if (!raw || !out) return false;

    // boş kontrolü (en azından ilk byte 0xFF ise boş say)
    if (raw[0] == 0xFF) return false;

    uint8_t expected = calculate_crc8(raw, 2);
    if (expected != raw[2]) {
        ESP_LOGW(TAG, "CRC mismatch in default duration entry (expected=0x%02X, got=0x%02X)", expected, raw[2]);
        return false;
    }

    out->duration = le16(&raw[0]);
    out->crc = raw[2];
    return true;
}

esp_err_t init_default_duration_partition() {
    duration_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, DURATION_PARTITION_SUBTYPE, DURATION_PARTITION_NAME);

    if (!duration_partition) {
        ESP_LOGE(TAG, "Duration partition is not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Duration partition is successfuly initiated.");

    return ESP_OK;
}

bool default_duration_read_last(DurationEntry *out) {
    if (!out) return false;
    if (!duration_partition && init_default_duration_partition() != ESP_OK) {
        return false;
    }
    int maxc = max_entry_count();
    DurationEntry last_ok = {0};
    bool any = false;

    for (int i = 0; i < maxc; i++) {
        uint8_t raw[DURATION_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(duration_partition, (size_t)i * DURATION_ENTRY_SIZE, raw, DURATION_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        // boş entry -> burada bitir
        if (raw[0] == 0xFF) break;

        DurationEntry tmp;
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

bool default_duration_append(uint16_t duration)
{
    duration = clamp_duration(duration);

    if (!duration_partition && init_default_duration_partition() != ESP_OK) return false;
    int maxc = max_entry_count();
    
    // (Opsiyonel) aynı entry zaten en sonda varsa tekrar yazma
    DurationEntry last;
    if (default_duration_read_last(&last)) {
        if (last.duration == duration ) {
            ESP_LOGI(TAG, "default_duration_append skipped (same as last)");
            return true;
        }
    }

    // ilk boş index'i bul
    int empty_idx = -1;
    for (int i = 0; i < maxc; i++) {
        uint8_t first;
        esp_err_t e = esp_partition_read(duration_partition, (size_t)i * DURATION_ENTRY_SIZE, &first, 1);
        if (e != ESP_OK) return false;
        if (first == 0xFF) {
            empty_idx = i;
            break;
        }
    }

    if (empty_idx < 0) {
        ESP_LOGE(TAG, "Duration partition is full. Need erase/rollover strategy.");
        return false;
    }

    uint8_t raw[DURATION_ENTRY_SIZE];
    encode_entry_bytes(duration, raw);

    esp_err_t we = esp_partition_write(duration_partition, (size_t)empty_idx * DURATION_ENTRY_SIZE, raw, DURATION_ENTRY_SIZE);
    if (we != ESP_OK) {
        ESP_LOGE(TAG, "default_duration_append write failed: %s", esp_err_to_name(we));
        return false;
    }

    ESP_LOGI(TAG, "default_duration_append ok idx=%d duration=%u", empty_idx, duration);
    return true;
}