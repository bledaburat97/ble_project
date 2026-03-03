#include "profile_partition_manager.h"

#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "log_utils.h"

#define PROFILE_PARTITION_NAME "profile_storage"
#define PROFILE_PARTITION_SUBTYPE 0x84
#define TAG "ProfilePartitionManager"
#define PROFILE_ENTRY_SIZE 7

esp_err_t erase_profile_partition(void)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x85, "passkey_storage");
    if (!p) {
        ESP_LOGE(TAG, "passkey_storage partition not found");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGW(TAG, "Erasing passkey_storage: addr=0x%08lX size=0x%lX",
             (unsigned long)p->address, (unsigned long)p->size);
    esp_err_t e = esp_partition_erase_range(p, 0, p->size);

    const esp_partition_t *p2 = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x86, "bright_storage");
    if (!p2) {
        ESP_LOGE(TAG, "bright_storage partition not found");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGW(TAG, "Erasing bright_storage: addr=0x%08lX size=0x%lX",
             (unsigned long)p2->address, (unsigned long)p2->size);
    e = esp_partition_erase_range(p2, 0, p2->size);


    const esp_partition_t *p3 = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x87, "duration_storage");
    if (!p3) {
        ESP_LOGE(TAG, "duration_storage partition not found");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGW(TAG, "Erasing duration_storage: addr=0x%08lX size=0x%lX",
             (unsigned long)p3->address, (unsigned long)p3->size);
    e = esp_partition_erase_range(p3, 0, p3->size);
    return e;
}

static const esp_partition_t* profile_partition = NULL;

static inline uint32_t le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static inline void wr_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static inline void wr_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static bool decode_entry_bytes(const uint8_t raw[PROFILE_ENTRY_SIZE], ProfileEntry *out)
{
    if (!raw || !out) return false;

    // boş kontrolü (en azından ilk byte 0xFF ise boş say)
    if (raw[0] == 0xFF) return false;

    uint8_t expected = calculate_crc8(raw, 6);
    if (expected != raw[6]) {
        ESP_LOGW(TAG, "CRC mismatch in profile entry (expected=0x%02X, got=0x%02X)", expected, raw[6]);
        return false;
    }

    out->profile_id = le32(&raw[0]);
    out->therapy_id = le16(&raw[4]);
    out->crc = raw[6];
    return true;
}

static void encode_entry_bytes(uint32_t profile_id, uint16_t therapy_id, uint8_t out_raw[PROFILE_ENTRY_SIZE])
{
    memset(out_raw, 0xFF, PROFILE_ENTRY_SIZE);
    wr_le32(&out_raw[0], profile_id);
    wr_le16(&out_raw[4], therapy_id);
    out_raw[6] = calculate_crc8(out_raw, 6);
}

static int max_entry_count(void)
{
    if (!profile_partition) return 0;
    return (int)(profile_partition->size / PROFILE_ENTRY_SIZE);
}

esp_err_t init_profile_partition() {
    profile_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, PROFILE_PARTITION_SUBTYPE, PROFILE_PARTITION_NAME);

    if (!profile_partition) {
        ESP_LOGE(TAG, "Profile partition is not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Profile partition is successfuly initiated.");

    return ESP_OK;
}

bool profile_read_at(int index, ProfileEntry* out)
{
    if (!out) return false;
    if (!profile_partition && init_profile_partition() != ESP_OK) return false;

    int maxc = max_entry_count();
    if (index < 0 || index >= maxc) return false;

    uint8_t raw[PROFILE_ENTRY_SIZE];
    esp_err_t e = esp_partition_read(profile_partition, (size_t)index * PROFILE_ENTRY_SIZE, raw, PROFILE_ENTRY_SIZE);
    if (e != ESP_OK) return false;

    return decode_entry_bytes(raw, out);
}

bool profile_read_last(ProfileEntry* out)
{
    if (!out) return false;
    if (!profile_partition && init_profile_partition() != ESP_OK) return false;

    int maxc = max_entry_count();
    ProfileEntry last_ok = {0};
    bool any = false;

    for (int i = 0; i < maxc; i++) {
        uint8_t raw[PROFILE_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(profile_partition, (size_t)i * PROFILE_ENTRY_SIZE, raw, PROFILE_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        // boş entry -> burada bitir
        if (raw[0] == 0xFF) break;

        ProfileEntry tmp;
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

bool profile_find_last_occurrence(uint32_t profile_id, ProfileEntry* out, int* out_index)
{
    if (!out || !out_index) return false;
    if (!profile_partition && init_profile_partition() != ESP_OK) return false;

    int maxc = max_entry_count();

    ProfileEntry last_match = {0};
    int last_idx = -1;

    for (int i = 0; i < maxc; i++) {
        uint8_t raw[PROFILE_ENTRY_SIZE];
        esp_err_t e = esp_partition_read(profile_partition, (size_t)i * PROFILE_ENTRY_SIZE, raw, PROFILE_ENTRY_SIZE);
        if (e != ESP_OK) return false;

        if (raw[0] == 0xFF) break;

        ProfileEntry tmp;
        if (!decode_entry_bytes(raw, &tmp)) {
            break;
        }

        if (tmp.profile_id == profile_id) {
            last_match = tmp;
            last_idx = i;
        }
    }

    if (last_idx < 0) return false;
    *out = last_match;
    *out_index = last_idx;
    return true;
}

bool profile_append(uint32_t profile_id, uint16_t therapy_id)
{
    if (profile_id == 0 || therapy_id == 0) {
        ESP_LOGW(TAG, "profile_append invalid args (profile_id=%lu, therapy_id=%u)",
                 (unsigned long)profile_id, therapy_id);
        return false;
    }

    if (!profile_partition && init_profile_partition() != ESP_OK) return false;

    int maxc = max_entry_count();

    // (Opsiyonel) aynı entry zaten en sonda varsa tekrar yazma
    ProfileEntry last;
    if (profile_read_last(&last)) {
        if (last.profile_id == profile_id && last.therapy_id == therapy_id) {
            ESP_LOGI(TAG, "profile_append skipped (same as last)");
            return true;
        }
    }

    // ilk boş index'i bul
    int empty_idx = -1;
    for (int i = 0; i < maxc; i++) {
        uint8_t first;
        esp_err_t e = esp_partition_read(profile_partition, (size_t)i * PROFILE_ENTRY_SIZE, &first, 1);
        if (e != ESP_OK) return false;
        if (first == 0xFF) {
            empty_idx = i;
            break;
        }
    }

    if (empty_idx < 0) {
        ESP_LOGE(TAG, "Profile partition is full. Need erase/rollover strategy.");
        return false;
    }

    uint8_t raw[PROFILE_ENTRY_SIZE];
    encode_entry_bytes(profile_id, therapy_id, raw);

    esp_err_t we = esp_partition_write(profile_partition, (size_t)empty_idx * PROFILE_ENTRY_SIZE, raw, PROFILE_ENTRY_SIZE);
    if (we != ESP_OK) {
        ESP_LOGE(TAG, "profile_append write failed: %s", esp_err_to_name(we));
        return false;
    }

    ESP_LOGI(TAG, "profile_append ok idx=%d profile=%lu therapy=%u", empty_idx, (unsigned long)profile_id, therapy_id);
    return true;
}