#include "log_writer.h"

#include "log_types.h"
#include "log_utils.h"
#include "log_partition_manager.h"
#include "therapy_counter.h"

#include "../state/deep_sleep_manager.h"

#include "../transaction/matching_message_encoder.h"

#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "LogWriter"

#define THERAPY_SLOT_SIZE 4096
#define MAX_SAVED_THERAPY 31
#define MAX_PENDING_LOGS 128

static BaseLogEntry pending_logs[MAX_PENDING_LOGS];
static size_t pending_log_count = 0;

static bool is_cached_logs_existed = false;
static uint8_t therapy_slot_buffer[THERAPY_SLOT_SIZE];
static uint8_t write_buffer[MAX_LOG_ENTRY_SIZE];
static uint8_t entry_buffer[MAX_LOG_ENTRY_SIZE];

static uint32_t starting_local_offset = 0;
static bool can_continue_uncompleted_therapy = false;

void set_continue_uncompleted_therapy(bool status) {
    can_continue_uncompleted_therapy = status;
}

static inline bool verify_crc(const uint8_t* entry_ptr, LogEntrySizeInfo entry_size_info) {
    if (entry_size_info.total_length < 4) return false; // en küçük kayıt 1(type)+0(data)+2(passed)+1(crc)
    uint8_t expected = calculate_crc8(entry_ptr, entry_size_info.total_length - 1);
    uint8_t actual   = entry_ptr[entry_size_info.total_length - 1];
    return expected == actual;
}

//Log’un flash’a hemen yazılmasına gerek yoksa yani bir terapi başlamamışsa cachelenir ve sonradan topluca yazılmak üzere bellekte tutulur.
static esp_err_t cache_log_entry(const BaseLogEntry* log) {
    if (!log) return ESP_ERR_INVALID_ARG;

    if (pending_log_count >= MAX_PENDING_LOGS) {
        ESP_LOGE(TAG, "Pending log cache FULL. Entering deep sleep...");
        enter_deep_sleep();
        return ESP_ERR_NO_MEM;
    }

    pending_logs[pending_log_count++] = *log;
    return ESP_OK;
}

//Verilen entry verisini offset konumuna flash’a yazar. Son bayta CRC8 (hata kontrolü) ekler.
//Flash’a doğrudan bir log yazarken kullanılır.
//Önce buffer sıfırlanır → veri kopyalanır → CRC eklenir → yazılır.
static esp_err_t write_log_entry(uint32_t offset, const uint8_t* entry, size_t size) {
    if (!entry || size == 0 || size > MAX_LOG_ENTRY_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(write_buffer, 0, MAX_LOG_ENTRY_SIZE);
    memcpy(write_buffer, entry, size);
    write_buffer[size - 1] = calculate_crc8(write_buffer, size - 1);
    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot write to flash, partition is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_partition_write(partition, offset, write_buffer, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Log entry written at offset %lu", offset);
    return ESP_OK;
}

//Flash slotunun dolduğunu belirten özel bir log (FLASH_SLOT_IS_FULL) oluşturup offset adresine yazar.
//Slot'a daha fazla log sığmayacaksa ve bu durum flash’a kaydedilmek isteniyorsa.
static esp_err_t write_slot_as_full(uint32_t offset, uint16_t entry_passed_seconds)
{
    Notification_t slot_full_entry = {
        .type = FLASH_SLOT_IS_FULL,
        .passed_seconds = entry_passed_seconds,
        .crc = 0
    };
    slot_full_entry.crc = calculate_crc8((uint8_t*)&slot_full_entry, sizeof(slot_full_entry) - 1);

    return write_log_entry(offset, (const uint8_t*)&slot_full_entry, sizeof(Notification_t));
}

//Verilen boyuttaki (entry_size) yeni bir log'u yazmak için uygun boş offset'i bulur.
//THERAPY_COMPLETED varsa yazılamaz
//FLASH_SLOT_IS_FULL varsa sadece THERAPY_COMPLETED yazılabilir
//Slotun neredeyse dolduğu durum tespit edilirse *is_slot_getting_full flag'i set edilir.
static bool find_next_log_offset(size_t entry_size, uint8_t entry_type, bool* is_slot_getting_full, uint32_t* out_local_offset) {
    if (!is_slot_getting_full) return false;

    uint32_t local_offset = starting_local_offset;
    *is_slot_getting_full = false;

    while (local_offset + entry_size <= THERAPY_SLOT_SIZE) {
        uint8_t existing_type = therapy_slot_buffer[local_offset];

        if (existing_type == FLASH_SLOT_IS_FULL) {
            if (entry_type != NOTIF_THERAPY_COMPLETED && entry_type != NOTIF_THERAPY_STOPPED_BY_APP) {
                ESP_LOGI(TAG, "There is no space left at slot.");
                return false;
            }
        }

        else if(existing_type == NOTIF_THERAPY_COMPLETED || existing_type == NOTIF_THERAPY_STOPPED_BY_APP) {
            //ESP_LOGE(TAG, "This slot with local offset: %lu was completed.", local_offset);
            return false;
        }

        else if (existing_type == 0xFF) {
            if(entry_type != NOTIF_THERAPY_COMPLETED && local_offset + entry_size > THERAPY_SLOT_SIZE - 2 * sizeof(Notification_t)){
                *is_slot_getting_full = true;
            }
            *out_local_offset = local_offset;
            starting_local_offset = local_offset;
            return true;
        }

        LogEntrySizeInfo size_info = get_log_entry_size_info(existing_type);
        if (local_offset + size_info.total_length > THERAPY_SLOT_SIZE) {
            return false; // Bozulmuş log veya taşma
        }

        local_offset += size_info.total_length;
    }

    return false;
}

//Log verisini flash’a yazılabilir formatta byte dizisine dönüştürmek için.
static bool create_log_entry(const BaseLogEntry* log, uint8_t* out_entry) {
    if(!log) {
        ESP_LOGE(TAG, "There is no log");
        return false;
    }
    if(!out_entry) {
        ESP_LOGE(TAG, "There is no out_entry");
        return false;
    }
    if(log->entry_size < 4) {
        ESP_LOGE(TAG, "entry_size can not be smaller than 4, entry_size is: %u", log->entry_size);
        return false;
    }

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


static bool read_max_passed_in_slot(uint32_t base_offset, uint16_t *out_max_passed) {
    if (!out_max_passed) return false;

    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return false;
    }
    esp_err_t err = esp_partition_read(partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at base_offset %lu: %s", base_offset, esp_err_to_name(err));
        return false;
    }

    uint32_t local_offset = 0;
    uint16_t max_passed = 0;
    bool any = false;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];
        if (type == 0xFF) break;

        LogEntrySizeInfo si = get_log_entry_size_info(type);
        if (si.total_length == 0 || local_offset + si.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Corrupt or overflow log entry at local_offset %lu", local_offset);
            break;
        }

        uint16_t cur_passed =
            (therapy_slot_buffer[local_offset + si.total_length - 3] << 8) |
             therapy_slot_buffer[local_offset + si.total_length - 2];

        if (cur_passed > max_passed) {
            max_passed = cur_passed;
            any = true;
        }

        local_offset += si.total_length;
    }

    if (any) {
        *out_max_passed = max_passed;
        return true;
    }
    return false;
}

static esp_err_t load_therapy_slot(uint32_t base_offset) {
    const esp_partition_t* log_partition = get_log_partition();
    if (!log_partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_partition_read(log_partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
    }
    return err;
}

/*
Tüm log yazma sürecini yöneten merkezi fonksiyon:
    create_log_entry ile byte array oluşturur
    Boyut ve CRC doğrulaması yapar
    Slot’taki uygun yeri bulmak için find_next_log_offset çağırır
    Slot dolmak üzereyse → write_slot_as_full çağrılır
    Yeterli yer varsa → write_log_entry ile log yazılır
    Slot tam dolmuşsa log yazılmaz
    last_saved_passed_duration güncellenir
    Hatalı girişlere karşı boyut/CRC koruması sağlar
*/
static esp_err_t append_log_entry_with_loaded_slot(uint32_t offset, const BaseLogEntry* log)
{
    //ESP_LOGI(TAG, "appending log entry (internal) with size: %u", log->entry_size);
    memset(entry_buffer, 0, MAX_LOG_ENTRY_SIZE);
    if (!create_log_entry(log, entry_buffer)) {
        ESP_LOGE(TAG, "Failed to create log entry from BaseLogEntry");
        return ESP_ERR_INVALID_ARG;
    }

    size_t expected_size = get_log_entry_size_info(log->type).total_length;
    ESP_LOGI(TAG, "Saving log with type:%u and expected size:%u", log->type, expected_size);

    if (expected_size == 0 || expected_size > MAX_LOG_ENTRY_SIZE) {
        ESP_LOGE(TAG, "Invalid or oversized log type: type=0x%02X", log->type);
        return ESP_ERR_INVALID_ARG;
    }

    if (log->entry_size != expected_size) {
        ESP_LOGE(TAG, "Size mismatch: expected=%d, got=%d", expected_size, log->entry_size);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t expected_crc = calculate_crc8(entry_buffer, log->entry_size - 1);
    if (entry_buffer[log->entry_size - 1] != expected_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected=0x%02X, got=0x%02X", expected_crc, entry_buffer[log->entry_size - 1]);
    }

    uint32_t local_offset;
    bool is_slot_getting_full = false;

    bool found = find_next_log_offset(log->entry_size, log->type, &is_slot_getting_full, &local_offset);
    if (!found) {
        return ESP_ERR_NO_MEM;
    }

    if (is_slot_getting_full) {
        esp_err_t werr = write_slot_as_full(offset + local_offset, log->passed_seconds);
        if (werr == ESP_OK) {
            ESP_LOGI(TAG, "Slot is full written at offset %lu", offset + local_offset);
            starting_local_offset = local_offset + sizeof(Notification_t);
        } else {
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(werr));
            return werr;
        }
    } else {
        esp_err_t werr = write_log_entry(offset + local_offset, entry_buffer, log->entry_size);
        if (werr == ESP_OK) {
            starting_local_offset = local_offset + log->entry_size;
        } else {
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(werr));
            return werr;
        }
    }
    return ESP_OK;
}

static esp_err_t append_log_entry(uint32_t base_offset, const BaseLogEntry* log)
{
    esp_err_t err = load_therapy_slot(base_offset);
    if (err != ESP_OK) {
        return err;
    }
    return append_log_entry_with_loaded_slot(base_offset, log);
}

//RAM’de bekleyen tüm logları belirtilen flash adresine sırasıyla yazar.
//Örneğin bir terapi tamamlandığında veya belirli koşullar sağlandığında geçici logların hepsi flash'a aktarılır.
static esp_err_t flush_cached_logs_to_slot(uint32_t offset) {
    ESP_LOGI(TAG, "Flush cached logs. pending log count: %u", pending_log_count);

    esp_err_t err = load_therapy_slot(offset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load slot before flush: %s", esp_err_to_name(err));
        return err;
    }

    for (int i = 0; i < pending_log_count; i++) {
        err = append_log_entry_with_loaded_slot(offset, &pending_logs[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to flush log[%d]: %s", i, esp_err_to_name(err));
            return err;
        }

        // WDT ve scheduler'a nefes
        if ((i & 0x7) == 0) {   // her 8 log’da bir
            vTaskDelay(1);      // ~1 tick
        }
    }

    pending_log_count = 0;
    return ESP_OK;
}


//İstenilen flash slotunda istenilen log var mı diye kontrol edilir.
//Örneğin bir slot tamamlanmış mı (yani THERAPY_COMPLETED log'u var mı) diye anlamak için.
static bool does_slot_contain_entry(uint32_t base_offset, uint8_t type_of_entry) {
    uint32_t local_offset = 0;

    const esp_partition_t* log_partition = get_log_partition();
    if (!log_partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return false;
    }

    esp_err_t err = esp_partition_read(log_partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
        return false;
    }

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];

        if (type == 0xFF) break;

        if (type == type_of_entry) return true;

        size_t size = get_log_entry_size_info(type).total_length;
        if (size == 0 || local_offset + size > THERAPY_SLOT_SIZE) break;

        local_offset += size;
    }

    return false;
}

static bool is_pause_state_type(uint8_t type) {
    return (type == TIMER_STATE_PAUSED_THERAPY) ||
           (type == TIMER_STATE_LOW_TEMP_ALERT_1) ||
           (type == TIMER_STATE_HIGH_TEMP_ALERT_1) ||
           (type == TIMER_STATE_LOW_TEMP_ALERT_2) ||
           (type == TIMER_STATE_HIGH_TEMP_ALERT_2) ||
           (type == TIMER_STATE_LOW_TEMP_ALERT_3) ||
           (type == TIMER_STATE_HIGH_TEMP_ALERT_3) ||
           (type == TIMER_STATE_LOW_HUM_ALERT) ||
           (type == TIMER_STATE_HIGH_HUM_ALERT) ||
           (type == NOTIF_THERAPY_PAUSED_BY_BUTTON) ||
           (type == NOTIF_THERAPY_PAUSED_BY_APP);
}

// "koşan terapiyi" temsil eden state mi?
static bool is_start_or_continue_state(uint8_t type) {
    return (type == TIMER_STATE_NEW_THERAPY_BY_BUTTON) ||
           (type == TIMER_STATE_NEW_THERAPY_BY_APP) ||
           (type == TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON) ||
           (type == TIMER_STATE_CONTINUE_THERAPY_BY_APP);
}

/**
 * Son logun session passed'ını ve gerçek therapy_passed'i hesaplar.
 * - base_offset: ilgili terapi slotunun base offset'i
 * - out_last_session_passed: slot’taki son logun passed_seconds'ı
 * - out_therapy_passed: hesaplanmış therapy_passed_seconds
 */
static bool compute_uncompleted_therapy_passed(uint32_t base_offset,
                                               uint16_t *out_last_session_passed,
                                               uint16_t *out_therapy_passed)
{
    if (!out_last_session_passed || !out_therapy_passed) {
        return false;
    }

    const esp_partition_t *partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return false;
    }

    esp_err_t err = esp_partition_read(partition, base_offset,
                                       therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at base_offset %lu: %s",
                 base_offset, esp_err_to_name(err));
        return false;
    }

    uint32_t local_offset = 0;
    bool any_log = false;
    uint16_t last_session_passed = 0;

    bool last_start_state_found = false;
    uint16_t last_start_state_session_passed = 0;
    uint16_t last_start_state_therapy_passed = 0;
    bool last_pause_state_found = false;
    uint16_t last_pause_state_session_passed = 0;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];
        if (type == 0xFF) {
            break;
        }

        LogEntrySizeInfo si = get_log_entry_size_info(type);
        if (si.total_length == 0 ||
            local_offset + si.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Corrupt or overflow log entry at local_offset %lu",
                     local_offset);
            break;
        }

        const uint8_t *entry_ptr = &therapy_slot_buffer[local_offset];
        if (!verify_crc(entry_ptr, si)) {
            ESP_LOGE(TAG, "CRC mismatch at local_offset=%lu. Stopping read.",
                     local_offset);
            break;
        }

        // Son logun session passed_seconds'ı (tüm loglar için geçerli)
        uint16_t passed =
            (entry_ptr[si.total_length - 3] << 8) |
             entry_ptr[si.total_length - 2];

        last_session_passed = passed;
        any_log = true;

        // TIMER_STATE_* ise, therapy_passed'i de çekmeye çalış
        if (is_start_or_continue_state(type)) {
            const uint8_t first_data_byte_index = 1;
            const uint8_t *data_ptr = &entry_ptr[first_data_byte_index];

            uint16_t therapy_passed = 0;

            // TimerStateInfoMessage formatı:
            // data[0-1] = therapy_id
            // data[2-3] = duration
            // data[4-5] = therapy_passed_seconds
            if (si.data_length >= 6) {
                therapy_passed =
                    (data_ptr[4] << 8) | data_ptr[5];
            } else {
                // Eski kayıtlar için ya da henüz bu bilgiler eklenmemişse 0
                therapy_passed = 0;
            }
            ESP_LOGI(TAG, "Timer start found, type: %u, last_state_session_passed: %u, last_state_therapy_passed: %u", type, passed, therapy_passed);
            last_start_state_found = true;
            last_start_state_session_passed = passed;
            last_start_state_therapy_passed = therapy_passed;
        }

        else if (is_pause_state_type(type)) {
            ESP_LOGI(TAG, "Timer pause found, type: %u, last_state_session_passed: %u", type, passed);

            last_pause_state_found = true;
            last_pause_state_session_passed = passed;
        }

        local_offset += si.total_length;
    }

    if (!any_log) {
        return false;
    }

    uint16_t therapy_passed = 0;

    if (last_start_state_found) {
        if(last_pause_state_found && last_pause_state_session_passed >= last_start_state_session_passed) {
            therapy_passed = last_start_state_therapy_passed + last_pause_state_session_passed - last_start_state_session_passed;
            ESP_LOGI(TAG, "Start and paused found, therapy passed: %u", therapy_passed);
        }
        else {
            therapy_passed =  last_start_state_therapy_passed + last_session_passed - last_start_state_session_passed;
            ESP_LOGI(TAG, "Start found but paused not found, therapy passed: %u", therapy_passed);
        }
    } else {
        ESP_LOGI(TAG, "Start not found.");
        therapy_passed = 0;
    }

    *out_last_session_passed = last_session_passed;
    *out_therapy_passed = therapy_passed;
    return true;
}

bool read_uncompleted_therapy(UncompletedTherapyInfo *out) {
    if (!out) {
        return false;
    }

    uint16_t therapy_count = read_therapy_count();
    if (therapy_count == 0) {
        // Daha önce hiç terapi yok
        return false;
    }

    uint32_t base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;

    bool slot_is_finished =
        does_slot_contain_entry(base_offset, NOTIF_THERAPY_COMPLETED)   ||
        does_slot_contain_entry(base_offset, NOTIF_THERAPY_STOPPED_BY_APP) ||
        does_slot_contain_entry(base_offset, NOTIF_ENTER_DEEP_SLEEP);

    if (slot_is_finished) {
        ESP_LOGI(TAG, "Last therapy slot is already finished. No uncompleted therapy.");
        return false;
    }

    ReadTherapyInfo info = {0};
    if (!read_therapy_info(therapy_count, &info, false)) {
        ESP_LOGE(TAG, "Failed to read therapy info for last therapy_id=%u", therapy_count);
        return false;
    }

    // Başlangıç logu yoksa (duration=0) devam edecek bir terapi de yok
    if (info.therapy_duration == 0) {
        ESP_LOGW(TAG, "Last therapy slot has no valid start log. Skipping resume.");
        return false;
    }

    uint16_t last_session_passed = 0;
    uint16_t therapy_passed = 0;
    if (!compute_uncompleted_therapy_passed(base_offset,
                                            &last_session_passed,
                                            &therapy_passed)) {
        ESP_LOGW(TAG, "Failed to compute uncompleted therapy passed time. Fallback to info.passed_duration.");
        last_session_passed = info.passed_duration;
        therapy_passed = info.passed_duration;
    }

    memset(out, 0, sizeof(*out));
    out->therapy_id             = therapy_count;
    out->therapy_duration       = info.therapy_duration;
    out->last_passed_seconds    = last_session_passed;    // session-based
    out->therapy_passed_seconds = therapy_passed;         // gerçek terapi süresi
    memcpy(out->last_brightness, info.brightness, 6);

    ESP_LOGI(TAG,
             "Found uncompleted therapy: id=%u, duration=%u, last_session=%u, therapy_passed=%u",
             out->therapy_id,
             out->therapy_duration,
             out->last_passed_seconds,
             out->therapy_passed_seconds);

    return true;
}

/*
Logu oluşturur ve özelliğine bakılır:
    can_be_cached → cache_log_entry() ile RAM’e alınır.
    can_flush_cache → cache’te bekleyen tüm loglar flash’a yazılır.
    can_be_flashed → doğrudan append_log_entry() ile flash’a yazılır.
    can_start_cache → cache oturumu başlatılır (is_cached_logs_existed = true).
Ek işlevler:
    Eğer son slot tamamlanmışsa (THERAPY_COMPLETED varsa) → yeni bir terapi kaydı (slot) başlatılır.
    Yeni bir slota geçilirken o slot boştaki son slot ise en eski slot silinip yeniden kullanılabilir hale getirilir.
*/


esp_err_t add_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds)
{
    BaseLogEntry log = fill_base_log(type, data, data_len, passed_seconds);

    if (log.entry_size == 0) {
        ESP_LOGE(TAG, "Failed to fill log, skipping add_log");
        return ESP_FAIL;
    }

    // Yeni terapi başlangıcı: passed_seconds = 0 ve cache flush triger'ı
    bool is_new_therapy_start = (log.passed_seconds == 0) && (log.can_flush_cache);

    // --- 1) Yeni terapi başlatan loglar (TIMER_STATE_NEW_THERAPY_BY_*) ---
    if (is_new_therapy_start) {
        ESP_LOGI(TAG, "New therapy start log detected (type=%u).", log.type);

        uint16_t therapy_count = read_therapy_count();
        uint32_t old_base_offset = 0;

        // 1-a) Eski slotu finalize et (varsa)
        if (therapy_count > 0) {
            old_base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
            ESP_LOGI(TAG, "Old slot base offset: %lu", old_base_offset);

            bool slot_is_finished =
                does_slot_contain_entry(old_base_offset, NOTIF_THERAPY_COMPLETED) ||
                does_slot_contain_entry(old_base_offset, NOTIF_THERAPY_STOPPED_BY_APP) ||
                does_slot_contain_entry(old_base_offset, NOTIF_ENTER_DEEP_SLEEP);

            if (!slot_is_finished) {
                uint16_t last_passed = 0;
                bool have_last = read_max_passed_in_slot(old_base_offset, &last_passed);

                if (have_last && last_passed > 0) {
                    ESP_LOGW(TAG,
                        "Previous therapy slot is not finished. Injecting NOTIF_ENTER_DEEP_SLEEP at passed=%u.",
                        last_passed);

                    BaseLogEntry shutdown_log =
                        fill_base_log(NOTIF_ENTER_DEEP_SLEEP, NULL, 0, last_passed);
                    if (shutdown_log.entry_size == 0) {
                        ESP_LOGE(TAG, "Failed to create shutdown log entry");
                        return ESP_FAIL;
                    }

                    esp_err_t sh_err = append_log_entry(old_base_offset, &shutdown_log);
                    if (sh_err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to append shutdown log: %s", esp_err_to_name(sh_err));
                        return sh_err;
                    }
                } else {
                    ESP_LOGW(TAG,
                        "Previous therapy slot has no valid last_passed. Skipping SHUT_DOWN injection.");
                }
            }
        }

        // 1-b) Yeni terapi id'sini oluştur
        uint16_t new_therapy_count = therapy_count + 1;
        esp_err_t count_err = write_therapy_count(new_therapy_count);
        ESP_LOGI(TAG, "New Therapy Count: %u", new_therapy_count);
        if (count_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update therapy counter");
            return count_err;
        }

        // 1-c) Yeni slotun base offset'i
        uint32_t new_base_offset =
            ((new_therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
        starting_local_offset = 0;
        ESP_LOGI(TAG, "New slot base offset: %lu", new_base_offset);

        // 1-d) Ring buffer dolduysa overwrite edilecek slotu önceden sil
        if (new_therapy_count >= MAX_SAVED_THERAPY) {
            uint32_t deleting_slot_offset =
                (new_therapy_count % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
            ESP_LOGI(TAG, "Erasing old slot before overwrite. Offset: %lu", deleting_slot_offset);
            const esp_partition_t *log_partition = get_log_partition();
            if (!log_partition) {
                ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
                return ESP_ERR_INVALID_ARG;
            }

            esp_err_t erase_err = esp_partition_erase_range(log_partition, deleting_slot_offset, THERAPY_SLOT_SIZE);
            if (erase_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to erase therapy slot before overwriting: %s",
                         esp_err_to_name(erase_err));
                return erase_err;
            }
        }

        // 1-e) Cache'te log varsa yeni slota flush et
        if (is_cached_logs_existed && pending_log_count > 0) {
            ESP_LOGI(TAG, "Saved cache is flushing to new slot with base offset: %lu", new_base_offset);
            esp_err_t flush_err = flush_cached_logs_to_slot(new_base_offset);
            if (flush_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to flush cached logs: %s", esp_err_to_name(flush_err));
                return flush_err;
            }
        }
        is_cached_logs_existed = false; // flush sonrası cache devre dışı

        // 1-f) Bu TIMER_STATE_NEW_THERAPY_* logunu da yeni slota yaz
        return append_log_entry(new_base_offset, &log);
    }

    //ESP_LOGI(TAG, "Passed seconds: %u", log.passed_seconds);
    //ESP_LOGI(TAG, "Log type: %u", log.type);

    if (can_continue_uncompleted_therapy && (log.type == TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON || log.type == TIMER_STATE_CONTINUE_THERAPY_BY_APP)) {
        uint16_t therapy_count = read_therapy_count();
        if (therapy_count == 0) {
            ESP_LOGE(TAG, "CONTINUE_THERAPY log received but therapy_count is 0");
            return ESP_FAIL;
        }

        uint32_t old_base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;

        if (is_cached_logs_existed && pending_log_count > 0) {
            ESP_LOGI(TAG, "Flushing cached logs to existing slot with base offset: %lu", old_base_offset);
            esp_err_t flush_err = flush_cached_logs_to_slot(old_base_offset);
            if (flush_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to flush cached logs: %s", esp_err_to_name(flush_err));
                return flush_err;
            }
        }

        is_cached_logs_existed = false;
        return append_log_entry(old_base_offset, &log);
    }


    // --- 2) Yeni terapi başlangıcı DEĞİL, ama cache açıkken gelen loglar ---
    if (is_cached_logs_existed) {
        if (log.can_be_cached) {
            ESP_LOGI(TAG, "Log is cached (type=%u).", log.type);
            ESP_LOGE(TAG, "new log passed seconds: %u", log.passed_seconds);
            return cache_log_entry(&log);
        }

        // Normalde PASSED_DURATION_UPDATED gibi loglar can_be_cached=false.
        // Bunlar terapi içindeyken gelir; o sırada is_cached_logs_existed genelde false olur.
        // Yine de güvenli olsun diye:
        ESP_LOGW(TAG,
                 "Cache is active but log (type=%u) is not cacheable. Ignoring special handling.",
                 log.type);
        // Aşağıdaki normal flash akışına düşecek.
    }

    // --- 3) Cache açık değilken normal flash akışı ---
    if (log.can_be_flashed) {
        uint16_t therapy_count = read_therapy_count();
        if (therapy_count == 0) {
            ESP_LOGE(TAG, "There should be a saved therapy (therapy_count == 0).");
            return ESP_FAIL;
        }

        uint32_t base_offset =
            ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;

        if (log.can_start_cache) {
            ESP_LOGI(TAG, "Cache is starting with a flash log (type=%u).", log.type);
            is_cached_logs_existed = true;
            // NOT: Bu log flash'a yazılıyor, pending cache'e eklemiyoruz.
        }
        ESP_LOGI(TAG, "New log to be appended passed seconds: %u", log.passed_seconds);

        return append_log_entry(base_offset, &log);
    }

    // --- 4) Flash edilmeyecek ama cache başlatan loglar (ör: DEVICE_AWAKED) ---
    if (log.can_start_cache) {
        ESP_LOGI(TAG, "Cache logging is started (type=%u).", log.type);
        is_cached_logs_existed = true;
        ESP_LOGE(TAG, "new log passed seconds: %u", log.passed_seconds);
        return cache_log_entry(&log);
    }

    // Ne flash, ne cache: sadece yoksay (şu anki tipler için pek yok ama güvenli)
    ESP_LOGI(TAG, "Log (type=%u) is neither flashed nor cached. Ignoring.", log.type);
    return ESP_OK;
}


esp_err_t add_notification_log(uint8_t type, uint16_t passed_seconds) {
    ESP_LOGI(TAG, "Log of notification type of %u is being added with passed_seconds: %u", type, passed_seconds);
    uint8_t* data = NULL;
    return add_log(type, data, 0, passed_seconds);
}

//for test
void erase_therapy_partition(uint32_t offset) {
    const esp_partition_t *log_partition = get_log_partition();
    if (!log_partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return;
    }
    esp_err_t erase_err = esp_partition_erase_range(log_partition, offset, THERAPY_SLOT_SIZE);
    if (erase_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase therapy slot before overwriting: %s", esp_err_to_name(erase_err));
    }
    else {
        ESP_LOGI(TAG, "Therapy partition successfully erased.");
    }
}

bool read_records(uint16_t therapy_id, ReadTherapyLogs* therapy_logs, LogReadMode log_read_mode) {
    ESP_LOGI(TAG, "Read records for therapy id: %u", therapy_id);
    uint32_t base_offset = ((therapy_id - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Log partition is not initialized.");
        return false;
    }

    esp_err_t err = esp_partition_read(partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at index %u: %s", therapy_id, esp_err_to_name(err));
        return false;
    }

    therapy_logs->measurements = malloc(MAX_MEASUREMENT_LOGS * 4);
    therapy_logs->notifications = malloc(MAX_NOTIFICATION_LOGS * 3);
    therapy_logs->brightness_updates = malloc(MAX_BRIGHTNESS_LOGS * 8);

    if (!therapy_logs->measurements || !therapy_logs->notifications || !therapy_logs->brightness_updates) 
    {
        ESP_LOGE(TAG, "Memory allocation failed");
        return false;
    }

    therapy_logs->count_measurements = 0;
    therapy_logs->count_notifications = 0;
    therapy_logs->count_brightness = 0;
    
    uint16_t count_measurements = 0;
    uint16_t count_notifications = 0;
    uint16_t count_brightness = 0;

    uint32_t local_offset = 0;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];
        if (type == 0xFF) break;
        
        LogEntrySizeInfo size_info = get_log_entry_size_info(type);
        if (local_offset + size_info.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Wrong log entry.");
            return false;
        }

        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
        if (!verify_crc(entry_ptr, size_info)) {
            ESP_LOGE(TAG, "CRC mismatch at local_offset=%lu. Stopping read.", local_offset);
            break;
        }

        const uint8_t first_data_byte_index = 1;
        const uint8_t first_passed_duration_byte_index = first_data_byte_index + size_info.data_length;
        const uint8_t* data_ptr = &entry_ptr[first_data_byte_index];  // type'ten sonra gelen data

        switch (type) {
            case MEASUREMENT_CHANGED:
                if (count_measurements >= MAX_MEASUREMENT_LOGS) break;

                memcpy(&therapy_logs->measurements[count_measurements * 4], data_ptr, size_info.data_length); // data
                therapy_logs->measurements[count_measurements * 4 + size_info.data_length] = entry_ptr[first_passed_duration_byte_index]; // passed_seconds (1st byte)
                therapy_logs->measurements[count_measurements * 4 + size_info.data_length + 1] = entry_ptr[first_passed_duration_byte_index + 1]; // passed_seconds (2nd byte)
                count_measurements++;

                break;
            case NOTIF_BRIGHTNESS_UPDATED:
                if (count_brightness >= MAX_BRIGHTNESS_LOGS) break;

                memcpy(&therapy_logs->brightness_updates[count_brightness * 8], data_ptr, 6);
                therapy_logs->brightness_updates[count_brightness * 8 + size_info.data_length] = entry_ptr[first_passed_duration_byte_index];
                therapy_logs->brightness_updates[count_brightness * 8 + size_info.data_length + 1] = entry_ptr[first_passed_duration_byte_index + 1];
                count_brightness++;

                break;

            case PASSED_DURATION_UPDATED:
            case FLASH_SLOT_IS_FULL:
                break;
            case BLE_CONNECTED:
                if (count_notifications >= MAX_NOTIFICATION_LOGS) break;

                therapy_logs->notifications[count_notifications * 3] = type; // hata var mı 
                therapy_logs->notifications[count_notifications * 3 + 1] = entry_ptr[first_passed_duration_byte_index]; // passed_seconds
                therapy_logs->notifications[count_notifications * 3 + 2] = entry_ptr[first_passed_duration_byte_index + 1]; // passed_seconds
                count_notifications++;

                if (log_read_mode == READ_UNTIL_FIRST_BLE_CONNECTED) {
                    therapy_logs->count_notifications = count_notifications;
                    therapy_logs->count_brightness = count_brightness;
                    therapy_logs->count_measurements = count_measurements;
                }

                break;
            default:
                if (count_notifications >= MAX_NOTIFICATION_LOGS) break;

                therapy_logs->notifications[count_notifications * 3] = type; // hata var mı 
                therapy_logs->notifications[count_notifications * 3 + 1] = entry_ptr[first_passed_duration_byte_index]; // passed_seconds
                therapy_logs->notifications[count_notifications * 3 + 2] = entry_ptr[first_passed_duration_byte_index + 1]; // passed_seconds
                count_notifications++;

                break;
        }
        local_offset += size_info.total_length;
    }

    if (log_read_mode == READ_FULL_SLOT) {
        therapy_logs->count_notifications = count_notifications;
        therapy_logs->count_brightness = count_brightness;
        therapy_logs->count_measurements = count_measurements;
    }

    if (therapy_logs->count_notifications == 0 && therapy_logs->count_brightness == 0 && therapy_logs->count_measurements == 0) {
        return false;
    }
    ESP_LOGI(TAG, "Read records for therapy id: %u is successful.", therapy_id);
    return true;

}

bool read_therapy_info(uint16_t therapy_id, ReadTherapyInfo* therapy_info, bool slot_already_loaded) {
    uint32_t base_offset = ((therapy_id - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return false;
    }

    if (!slot_already_loaded) {
        esp_err_t err = esp_partition_read(partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read slot at index %u: %s", therapy_id, esp_err_to_name(err));
            return false;
        }
    }

    therapy_info->therapy_duration = 0;
    therapy_info->passed_duration = 0;
    therapy_info->is_over = false;

    uint32_t local_offset = 0;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];
        if (type == 0xFF) break;
        
        LogEntrySizeInfo size_info = get_log_entry_size_info(type);
        if (local_offset + size_info.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Wrong log entry.");
            return false;
        }

        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
        if (!verify_crc(entry_ptr, size_info)) {
            ESP_LOGE(TAG, "CRC mismatch at local_offset=%lu. Stopping read.", local_offset);
            break;
        }

        therapy_info->passed_duration = (therapy_slot_buffer[local_offset + size_info.total_length - 3] << 8 ) | therapy_slot_buffer[local_offset + size_info.total_length - 2]; 

        const uint8_t first_data_byte_index = 1;
        const uint8_t* data_ptr = &entry_ptr[first_data_byte_index];  // type'ten sonra gelen data

        switch (type) {
            case NOTIF_BRIGHTNESS_UPDATED:
                memcpy(therapy_info->brightness, data_ptr, 6);
                break;
            case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
            case TIMER_STATE_NEW_THERAPY_BY_APP:
            {
                if(therapy_info->therapy_duration > 0) {
                    ESP_LOGE(TAG, "Therapy with same therapy id is started more than once.");
                    return false; 
                }
                uint16_t t_id = (data_ptr[0] << 8) | data_ptr[1];
                uint16_t t_dur = (data_ptr[2] << 8) | data_ptr[3];
                if (therapy_id != t_id) {
                    ESP_LOGE(TAG, "Wrong therapy id is saved.: %u", t_id);
                    return false;
                }
                therapy_info->therapy_duration = t_dur;
                
                break;
            }
            case NOTIF_ENTER_DEEP_SLEEP:
            case NOTIF_THERAPY_STOPPED_BY_APP:
            case NOTIF_THERAPY_COMPLETED:
                therapy_info->is_over = true;
                break;
    
            default:
                break;
        }

        local_offset += size_info.total_length;
    }

    return true;
}

void read_and_print_test_logs(uint8_t therapy_id) {
    ESP_LOGI(TAG, "Starting log reading test...");

    uint32_t base_offset = ((therapy_id - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return;
    }
    esp_err_t err = esp_partition_read(partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at index %u: %s", therapy_id, esp_err_to_name(err));
        return;
    }

    uint32_t local_offset = 0;
    uint16_t log_count = 0;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];

        if (type == 0xFF) {
            ESP_LOGI(TAG, "End of logs reached. (0xFF terminator found)");
            break;
        }
        
        LogEntrySizeInfo size_info = get_log_entry_size_info(type);

        if (local_offset + size_info.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Corrupt log entry detected at offset %lu. Aborting read.", local_offset);
            break;
        }

        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
        const uint8_t* data_ptr = &entry_ptr[1];
        uint16_t passed_seconds = (entry_ptr[1 + size_info.data_length] << 8) | entry_ptr[1 + size_info.data_length + 1];
        uint8_t crc = entry_ptr[size_info.total_length - 1];
        
        log_count++;

        switch (type) {
            case MEASUREMENT_CHANGED:
                ESP_LOGI(TAG, "MEASUREMENT_CHANGED");
                ESP_LOGI(TAG, "Temp: %u, Hum: %u", data_ptr[0], data_ptr[1]);
                break;
            case NOTIF_BRIGHTNESS_UPDATED:
                ESP_LOGI(TAG, "NOTIF_BRIGHTNESS_UPDATED");
                ESP_LOGI(TAG, "Brightness: [%u, %u, %u, %u, %u, %u]", 
                        data_ptr[0], data_ptr[1], data_ptr[2], data_ptr[3], data_ptr[4], data_ptr[5]);
                break;
            case TIMER_STATE_NEW_THERAPY_BY_APP:
                ESP_LOGI(TAG, "TIMER_STATE_NEW_THERAPY_BY_APP");
                uint16_t therapy_id_read;
                uint16_t therapy_duration_read;
                memcpy(&therapy_id_read, &data_ptr[0], sizeof(therapy_id_read));
                memcpy(&therapy_duration_read, &data_ptr[2], sizeof(therapy_duration_read));
                ESP_LOGI(TAG, "Therapy ID: %u, Duration: %u", therapy_id_read, therapy_duration_read);
                break;
            case NOTIF_THERAPY_COMPLETED:
                ESP_LOGI(TAG, "NOTIF_THERAPY_COMPLETED");
                break;
            case DEVICE_AWAKED:
                ESP_LOGI(TAG, "DEVICE_AWAKED");
                break;
            case NOTIF_HELMET_ON:
                ESP_LOGI(TAG, "NOTIF_HELMET_ON");
                break;
            default:
                ESP_LOGI(TAG, "UNKNOWN_TYPE (%u)", type);
                break;
        }

        ESP_LOGI(TAG, "--- Log #%u ---", log_count);
        ESP_LOGI(TAG, "Passed Seconds: %u", passed_seconds);
        ESP_LOGI(TAG, "CRC: 0x%02X", crc);

        local_offset += size_info.total_length;
    }
    ESP_LOGI(TAG, "All logs have been read from the buffer.");
}