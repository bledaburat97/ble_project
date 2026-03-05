#include "log_orchestrator.h"

#include <string.h>

#include "esp_log.h"

#include "log_storage.h"
#include "log_cache.h"
#include "log_reader.h"
#include "log_utils.h"
#include "log_types.h"
#include "log_config.h"

#define TAG "LogOrchestrator"
/**
 * log_orchestrator:
 * - cihaza kaydedilmek istenen loglar için tek entrypoint
 */

// Terapi başlamadan önce gelen logları RAM'de tutan cache.
static LogCache s_cache;

static uint8_t s_slot_buf[THERAPY_SLOT_SIZE];
static uint8_t s_entry_buf[MAX_LOG_ENTRY_SIZE];
static uint8_t s_write_buf[MAX_LOG_ENTRY_SIZE];

// Slot içinde yazmaya başlanacak local offset (son yazım noktasından devam).
static uint32_t s_starting_local_offset = 0; // performans için; istenirse her seferinde scan ile bulunabilir.

// Entry CRC doğrulaması (bozuk kayıtları tespit etmek için).
static inline bool verify_crc(const uint8_t *entry_ptr, LogEntrySizeInfo si)
{
    if (!entry_ptr) return false;
    if (si.total_length < 4) return false;
    uint8_t expected = calculate_crc8(entry_ptr, si.total_length - 1);
    return expected == entry_ptr[si.total_length - 1];
}

// BaseLogEntry -> flash'a yazılacak ham entry formatı.
static bool create_log_entry_bytes(const BaseLogEntry *log, uint8_t *out_entry)
{
    if (!log || !out_entry) return false;
    if (log->entry_size < 4 || log->entry_size > MAX_LOG_ENTRY_SIZE) return false;

    out_entry[0] = log->type;

    size_t data_len = log->entry_size - 4;
    if (data_len > 0) {
        memcpy(&out_entry[1], log->data, data_len);
    }

    out_entry[log->entry_size - 3] = (log->passed_seconds >> 8) & 0xFF;
    out_entry[log->entry_size - 2] = log->passed_seconds & 0xFF;
    out_entry[log->entry_size - 1] = calculate_crc8(out_entry, log->entry_size - 1);
    return true;
}

// Belirli offset'e tek bir log entry yazar.
static esp_err_t write_log_entry_at(uint32_t abs_offset, const uint8_t *entry, size_t size)
{
    if (!entry || size == 0 || size > MAX_LOG_ENTRY_SIZE) return ESP_ERR_INVALID_ARG;

    memset(s_write_buf, 0, sizeof(s_write_buf));
    memcpy(s_write_buf, entry, size);
    s_write_buf[size - 1] = calculate_crc8(s_write_buf, size - 1);

    esp_err_t e = log_storage_write(abs_offset, s_write_buf, size);
    return e;
}

// Slot doluluk sınırına gelindiğinde "FLASH_SLOT_IS_FULL" yazar.
static esp_err_t write_slot_full_marker(uint32_t abs_offset, uint16_t passed_seconds)
{
    Notification_t slot_full = {
        .type = FLASH_SLOT_IS_FULL,
        .passed_seconds = passed_seconds,
        .crc = 0
    };
    slot_full.crc = calculate_crc8((uint8_t *)&slot_full, sizeof(slot_full) - 1);
    return write_log_entry_at(abs_offset, (const uint8_t *)&slot_full, sizeof(Notification_t));
}

// Terapiyi "bitmiş" kabul eden log tipleri.
static inline bool is_end_log_type(uint8_t t)
{
    return (t == NOTIF_THERAPY_COMPLETED || t == NOTIF_THERAPY_STOPPED_BY_APP || t == NOTIF_ENTER_DEEP_SLEEP);
}

/**
 * Slot içinde bir sonraki yazma offset’i bulur
 * s_slot_buf, base_offset slot’u yüklenmiş olmalı.
 */
static bool find_next_log_offset(size_t entry_size, uint8_t entry_type, bool *out_getting_full, uint32_t *out_local)
{
    if (!out_getting_full || !out_local) return false;

    uint32_t local = s_starting_local_offset;
    *out_getting_full = false;

    while (local + entry_size <= THERAPY_SLOT_SIZE) {
        uint8_t existing_type = s_slot_buf[local];

        if (existing_type == FLASH_SLOT_IS_FULL) {
            if (!is_end_log_type(entry_type)) {
                return false;
            }
        } else if (is_end_log_type(existing_type)) {
            return false;
        } else if (existing_type == 0xFF) {
            if (!is_end_log_type(entry_type) &&
                local + entry_size > THERAPY_SLOT_SIZE - 2 * sizeof(Notification_t)) {
                *out_getting_full = true;
            }
            *out_local = local;
            s_starting_local_offset = local;
            return true;
        }

        LogEntrySizeInfo si = get_log_entry_size_info(existing_type);
        if (si.total_length == 0 || local + si.total_length > THERAPY_SLOT_SIZE) return false;

        local += si.total_length;
    }
    return false;
}

// Slotu RAM buffer'a yükler.
static esp_err_t load_slot(uint32_t base_offset)
{
    return log_storage_read_slot(base_offset, s_slot_buf, sizeof(s_slot_buf));
}

// Slot RAM'de hazırken yeni log'u sona ekler.
static esp_err_t append_log_entry_with_loaded_slot(uint32_t base_offset, const BaseLogEntry *log)
{
    if (!log) return ESP_ERR_INVALID_ARG;

    memset(s_entry_buf, 0, sizeof(s_entry_buf));
    if (!create_log_entry_bytes(log, s_entry_buf)) return ESP_ERR_INVALID_ARG;

    size_t expected = get_log_entry_size_info(log->type).total_length;
    if (expected == 0 || expected > MAX_LOG_ENTRY_SIZE) return ESP_ERR_INVALID_ARG;
    if (log->entry_size != expected) return ESP_ERR_INVALID_ARG;

    // CRC check (debug)
    uint8_t crc = calculate_crc8(s_entry_buf, log->entry_size - 1);
    if (crc != s_entry_buf[log->entry_size - 1]) {
        ESP_LOGW(TAG, "CRC mismatch while creating entry (type=%u)", log->type);
    }

    bool getting_full = false;
    uint32_t local = 0;
    if (!find_next_log_offset(log->entry_size, log->type, &getting_full, &local)) {
        return ESP_ERR_NO_MEM;
    }

    uint32_t abs = base_offset + local;
    if (getting_full) {
        esp_err_t e = write_slot_full_marker(abs, log->passed_seconds);
        if (e == ESP_OK) s_starting_local_offset = local + sizeof(Notification_t);
        return e;
    } else {
        esp_err_t e = write_log_entry_at(abs, s_entry_buf, log->entry_size);
        if (e == ESP_OK) s_starting_local_offset = local + log->entry_size;
        return e;
    }
}

// Slotu yükleyip log'u ekler.
static esp_err_t append_log_entry(uint32_t base_offset, const BaseLogEntry *log)
{
    esp_err_t e = load_slot(base_offset);
    if (e != ESP_OK) return e;
    return append_log_entry_with_loaded_slot(base_offset, log);
}

// Ring buffer mantığıyla bir sonraki gelecek terapi için slot temizliği yapar.
static esp_err_t prepare_next_slot_for_new_therapy(uint16_t new_therapy_count)
{
    if (new_therapy_count >= MAX_SAVED_THERAPY) {
        uint32_t deleting_slot_offset = ((uint32_t)(new_therapy_count % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;
        ESP_LOGI(TAG, "Erasing reserved empty slot. Offset=%lu", deleting_slot_offset);
        return log_storage_erase(deleting_slot_offset, THERAPY_SLOT_SIZE);
    }
    return ESP_OK;
}

// Log orchestrator ve cache'i başlatır.
esp_err_t log_orchestrator_init(void)
{
    esp_err_t e = log_storage_init();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Log storage could not be initiated.");
        return e;
    }
    log_cache_init(&s_cache);
    s_starting_local_offset = 0;
    return ESP_OK;
}

// Cache'deki logları ilgili slota topluca yazar.
static esp_err_t flush_cached_logs_to_slot(uint32_t base_offset)
{
    esp_err_t e = load_slot(base_offset);
    if (e != ESP_OK) return e;

    for (size_t i = 0; i < s_cache.count; i++) {
        e = append_log_entry_with_loaded_slot(base_offset, &s_cache.entries[i]);
        if (e != ESP_OK) return e;
    }

    log_cache_clear(&s_cache);
    return ESP_OK;
}

// Önceki terapi slotta bitiş logu yoksa o slota bitiş log'u ekleyerek güvenli kapatma yapar.
esp_err_t finalize_old_slot(uint16_t therapy_id) {
    uint32_t old_base_offset = ((uint32_t)((therapy_id - 1) % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;
    if (!log_storage_is_ready()) {
        esp_err_t error = log_orchestrator_init();
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Log orchestrator could not be initiated.");
            return error;
        }
    }

    log_storage_lock();

    esp_err_t load_error = load_slot(old_base_offset);
    if (load_error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load old slot: %s", esp_err_to_name(load_error));
        log_storage_unlock();
        return load_error;
    }

    bool finished =
        log_reader_slot_buf_contains_type(s_slot_buf, NOTIF_THERAPY_COMPLETED) ||
        log_reader_slot_buf_contains_type(s_slot_buf, NOTIF_THERAPY_STOPPED_BY_APP) ||
        log_reader_slot_buf_contains_type(s_slot_buf, NOTIF_ENTER_DEEP_SLEEP);

    if (!finished) {
        uint16_t last_passed = 0;
        if (log_reader_slot_buf_read_max_passed(s_slot_buf, &last_passed) && last_passed > 0) {
            ESP_LOGW(TAG, "Previous therapy slot is not finished. Injecting NOTIF_ENTER_DEEP_SLEEP at passed=%u.", last_passed);

            BaseLogEntry shutdown_log = fill_base_log(NOTIF_ENTER_DEEP_SLEEP, NULL, 0, last_passed);
            if (shutdown_log.entry_size == 0) {
                ESP_LOGE(TAG, "Failed to create shutdown log entry");
                log_storage_unlock();
                return ESP_FAIL;
            }

            esp_err_t shut_down_error = append_log_entry(old_base_offset, &shutdown_log);
            if (shut_down_error != ESP_OK) {
                ESP_LOGE(TAG, "Failed to append shutdown log: %s", esp_err_to_name(shut_down_error));
                log_storage_unlock();
                return shut_down_error;
            }
        }
    }
    
    log_storage_unlock();
    return ESP_OK;
}

// Yeni terapi başlarken cache'i flush edip ilk log'u yazar.
esp_err_t log_orchestrator_flush_logs(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds, uint16_t therapy_id, bool is_first_log_to_append) {
    BaseLogEntry log = fill_base_log(type, data, data_len, passed_seconds);
    if (log.entry_size == 0) return ESP_FAIL;

    if (!log_storage_is_ready()) {
        esp_err_t error = log_orchestrator_init();
        if (error != ESP_OK){
            ESP_LOGE(TAG, "Log orchestrator could not be initiated.");
            return error;
        } 
    }

    log_storage_lock();

    if(is_first_log_to_append) {
        esp_err_t pe = prepare_next_slot_for_new_therapy(therapy_id);
        if (pe != ESP_OK) {
            log_storage_unlock();
            ESP_LOGE(TAG, "New slot could not be prepared.");
            return pe;
        }
        s_starting_local_offset = 0;
    }

    uint32_t new_base_offset = ((uint32_t)((therapy_id - 1) % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;

    // cache flush
    if (log_cache_is_active(&s_cache) && log_cache_count(&s_cache) > 0) {
        esp_err_t flush_err = flush_cached_logs_to_slot(new_base_offset);
        if (flush_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to flush cached logs: %s", esp_err_to_name(flush_err));
            log_storage_unlock();
            return flush_err;
        }
    }
    log_cache_stop(&s_cache); // sadece stop yeterli (clear flush içinde yapıldı)

    // start log’u yaz
    esp_err_t ae = append_log_entry(new_base_offset, &log);
    log_storage_unlock();
    return ae;
}

// Normal akışta tek bir log ekler (cache/flash kararını içerir).
esp_err_t log_orchestrator_add_log(uint8_t type, const uint8_t *data, size_t data_len, uint16_t passed_seconds, uint16_t therapy_id) {
    BaseLogEntry log = fill_base_log(type, data, data_len, passed_seconds);
    if (log.entry_size == 0) return ESP_FAIL;

    if (!log_storage_is_ready()) {
        esp_err_t error = log_orchestrator_init();
        if (error != ESP_OK){
            ESP_LOGE(TAG, "Log orchestrator could not be initiated.");
            return error;
        } 
    }

    log_storage_lock();

    if (log_cache_is_active(&s_cache)) {
        if (log.can_be_cached) {
            esp_err_t ce = log_cache_push(&s_cache, &log);
            log_storage_unlock();
            return ce;
        }
        ESP_LOGW(TAG, "Cache is active but log (type=%u) is not cacheable. Ignoring special handling.", log.type);
        // cache aktif ama cachelenemez -> normal flash akışına düşer.
    }

    // Normal flash akışı
    if (log.can_be_flashed ) {
        if (therapy_id == 0) {
            log_storage_unlock();
            return ESP_FAIL;
        }
        uint32_t base_offset = ((uint32_t)((therapy_id - 1) % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;

        if (log.can_start_cache) {
            log_cache_start(&s_cache);
        }

        esp_err_t ae = append_log_entry(base_offset, &log);
        log_storage_unlock();
        return ae;
    }

    // Flash edilmeyecek ama cache başlatacak loglar
    if (log.can_start_cache) {
        log_cache_start(&s_cache);
        esp_err_t ce = log_cache_push(&s_cache, &log);
        log_storage_unlock();
        return ce;
    }

    log_storage_unlock();
    return ESP_OK;
}
