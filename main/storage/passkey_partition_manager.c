#include "passkey_partition_manager.h"

#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "log_utils.h"

#define PASSKEY_PARTITION_NAME "passkey_storage"
#define PASSKEY_PARTITION_SUBTYPE 0x85
#define TAG "PasskeyPartitionManager"
#define PASSKEY_ENTRY_SIZE 5

static const esp_partition_t* passkey_partition = NULL;

static inline void wr_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static int max_entry_count(void)
{
    if (!passkey_partition) return 0;
    return (int)(passkey_partition->size / PASSKEY_ENTRY_SIZE);
}

static void encode_entry_bytes(uint32_t passkey, uint8_t out_raw[PASSKEY_ENTRY_SIZE])
{
    if (passkey > 999999u) passkey = 999999u;
    memset(out_raw, 0xFF, PASSKEY_ENTRY_SIZE);
    wr_le32(&out_raw[0], passkey);
    out_raw[4] = calculate_crc8(out_raw, 4);
}

static inline uint32_t le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool decode_entry_bytes(const uint8_t raw[PASSKEY_ENTRY_SIZE], PasskeyEntry *out)
{
    if (!raw || !out) return false;

    // boş kontrolü (en azından ilk byte 0xFF ise boş say)
    if (raw[0] == 0xFF) return false;

    uint8_t expected = calculate_crc8(raw, 4);
    if (expected != raw[4]) {
        ESP_LOGW(TAG, "CRC mismatch in passkey entry (expected=0x%02X, got=0x%02X)", expected, raw[4]);
        return false;
    }

    out->passkey = le32(&raw[0]);
    out->crc = raw[4];
    return true;
}

esp_err_t init_passkey_partition() {
    passkey_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, PASSKEY_PARTITION_SUBTYPE, PASSKEY_PARTITION_NAME);

    if (!passkey_partition) {
        ESP_LOGE(TAG, "Passkey partition is not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Passkey partition is successfuly initiated.");

    return ESP_OK;
}

bool passkey_read_last(PasskeyEntry *out) {
    if (!out) return false;
    if (!passkey_partition && init_passkey_partition() != ESP_OK) {
        return false;
    }
    int maxc = max_entry_count();
    PasskeyEntry last_ok = {0};
    bool any = false;

    for (int i = 0; i < maxc; i++) {
        uint8_t raw[PASSKEY_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(passkey_partition, (size_t)i * PASSKEY_ENTRY_SIZE, raw, PASSKEY_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        // boş entry -> burada bitir
        if (raw[0] == 0xFF) break;

        PasskeyEntry tmp;
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

bool passkey_append(uint32_t passkey)
{
    if (!passkey_partition && init_passkey_partition() != ESP_OK) return false;
    int maxc = max_entry_count();
    
    // (Opsiyonel) aynı entry zaten en sonda varsa tekrar yazma
    PasskeyEntry last;
    if (passkey_read_last(&last)) {
        if (last.passkey == passkey ) {
            ESP_LOGI(TAG, "passkey_append skipped (same as last)");
            return true;
        }
    }

    // ilk boş index'i bul
    int empty_idx = -1;
    for (int i = 0; i < maxc; i++) {
        uint8_t first;
        esp_err_t e = esp_partition_read(passkey_partition, (size_t)i * PASSKEY_ENTRY_SIZE, &first, 1);
        if (e != ESP_OK) return false;
        if (first == 0xFF) {
            empty_idx = i;
            break;
        }
    }

    if (empty_idx < 0) {
        ESP_LOGE(TAG, "Passkey partition is full. Need erase/rollover strategy.");
        return false;
    }

    uint8_t raw[PASSKEY_ENTRY_SIZE];
    encode_entry_bytes(passkey, raw);

    esp_err_t we = esp_partition_write(passkey_partition, (size_t)empty_idx * PASSKEY_ENTRY_SIZE, raw, PASSKEY_ENTRY_SIZE);
    if (we != ESP_OK) {
        ESP_LOGE(TAG, "passkey_append write failed: %s", esp_err_to_name(we));
        return false;
    }

    ESP_LOGI(TAG, "passkey_append ok idx=%d paskey=%lu", empty_idx, (unsigned long)passkey);
    return true;
}

bool passkey_read_first(PasskeyEntry *out)
{
    if (!out) return false;
    if (!passkey_partition && init_passkey_partition() != ESP_OK) {
        return false;
    }

    // Append-only yapıda "ilk yazılan" = index 0 varsayımı.
    // Ancak üretimde bazen boş/bozuk olabilirse, ilk valid entry’yi tarayalım.
    int maxc = max_entry_count();
    for (int i = 0; i < maxc; i++) {
        uint8_t raw[PASSKEY_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(passkey_partition, (size_t)i * PASSKEY_ENTRY_SIZE,
                                         raw, PASSKEY_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        // boş entry -> daha ileri yok
        if (raw[0] == 0xFF) break;

        PasskeyEntry tmp;
        if (decode_entry_bytes(raw, &tmp)) {
            *out = tmp;
            return true;
        }
    }

    return false;
}