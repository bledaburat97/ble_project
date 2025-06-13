#include "log_writer.h"
#include "log_types.h"
#include "log_utils.h"
#include "therapy_counter.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "deep_sleep_manager.h"
#include "matching_message_encoder.h"

#define TAG "LogWriter"

#define MAX_PENDING_LOGS 128

static BaseLogEntry pending_logs[MAX_PENDING_LOGS];
static size_t pending_log_count = 0;

static bool is_cached_logs_existed = false;
static uint8_t therapy_slot_buffer[THERAPY_SLOT_SIZE];
static uint8_t write_buffer[MAX_LOG_ENTRY_SIZE];

static uint16_t last_saved_passed_duration = 0;
static uint32_t starting_local_offset = 0;

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


//İstenilen flash slotunda istenilen log var mı diye kontrol edilir.
//Örneğin bir slot tamamlanmış mı (yani THERAPY_COMPLETED log'u var mı) diye anlamak için.
static bool does_slot_contain_entry(uint32_t base_offset, uint8_t type_of_entry) {
    uint32_t local_offset = 0;

    esp_err_t err = esp_partition_read(get_log_partition(), base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
        return false;
    }

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];

        if (type == 0xFF) break;

        if (type == type_of_entry) return true;

        size_t size = get_log_entry_size(type);
        if (size == 0 || local_offset + size > THERAPY_SLOT_SIZE) break;

        local_offset += size;
    }

    return false;
}

/*
static void read_logs(uint32_t base_offset) {

    esp_err_t err = esp_partition_read(get_log_partition(), base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
    }
    uint32_t local_offset = 0;

    while (local_offset <= THERAPY_SLOT_SIZE) {
        if (therapy_slot_buffer[local_offset] == 0xFF) break;  // boş alan bulundu
        uint8_t type = therapy_slot_buffer[local_offset];
        ESP_LOGI(TAG, "Read log type is: %u", type);
        size_t entry_size = get_log_entry_size(type);
        if (entry_size == 0) break;

        local_offset += entry_size;
    }
    ESP_LOGI(TAG, "All logs are read.");

}
*/

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

    esp_err_t err = esp_partition_write(get_log_partition(), offset, write_buffer, size);
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
            if (entry_type != THERAPY_COMPLETED && entry_type != THERAPY_STOPPED_BY_APP) {
                ESP_LOGI(TAG, "There is no space left at slot.");
                return false;
            }
        }

        else if(existing_type == THERAPY_COMPLETED || existing_type == THERAPY_STOPPED_BY_APP) {
            ESP_LOGE(TAG, "This slot was completed.");
            return false;
        }

        else if (existing_type == 0xFF) {
            if(entry_type != THERAPY_COMPLETED && local_offset + entry_size > THERAPY_SLOT_SIZE - 2 * sizeof(Notification_t)){
                *is_slot_getting_full = true;
            }
            *out_local_offset = local_offset;
            starting_local_offset = local_offset;
            return true;
        }

        size_t existing_entry_size = get_log_entry_size(existing_type);
        if (existing_entry_size == 0 || local_offset + existing_entry_size > THERAPY_SLOT_SIZE) {
            return false; // Bozulmuş log veya taşma
        }

        local_offset += existing_entry_size;
    }

    return false;
}

//Log verisini flash’a yazılabilir formatta byte dizisine dönüştürmek için.
static bool create_log_entry(const BaseLogEntry* log, uint8_t* out_entry) {
    if(!log) {
        ESP_LOGI(TAG, "There is no log");
    }
    if(!out_entry) {
        ESP_LOGI(TAG, "There is no out_entry");
    }
    if(log->entry_size < 4) {
        ESP_LOGI(TAG, "entry_size can not be smaller than 4, entry_size is: %u", log->entry_size);
    }

    out_entry[0] = log->type;

    size_t data_len = log->entry_size - 4;
    if (data_len > 0) {
        memcpy(&out_entry[1], log->data, data_len);
    }
    else if (data_len == 0) {
        memset(&out_entry[1], 0, MAX_LOG_ENTRY_SIZE - 4);
    }
    else{
        ESP_LOGE(TAG, "data_len can not be smaller than zero.");
    }

    out_entry[log->entry_size - 3] = (log->passed_seconds >> 8) & 0xFF;
    out_entry[log->entry_size - 2] = log->passed_seconds & 0xFF;
    out_entry[log->entry_size - 1] = calculate_crc8(out_entry, log->entry_size - 1);

    return true;
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
esp_err_t append_log_entry(uint32_t offset, const BaseLogEntry* log) {
    uint8_t entry[MAX_LOG_ENTRY_SIZE] = {0};
    if (!create_log_entry(log, entry)) {
        ESP_LOGE(TAG, "Failed to create log entry from BaseLogEntry");
        return ESP_ERR_INVALID_ARG;
    }

    size_t expected_size = get_log_entry_size(log->type);
    ESP_LOGI(TAG, "Saving log with type:%u and expected size:%u", log->type, expected_size);

    if (expected_size == 0 || expected_size > MAX_LOG_ENTRY_SIZE) {
        ESP_LOGE(TAG, "Invalid or oversized log type: type=0x%02X", log->type);
        return ESP_ERR_INVALID_ARG;
    }

    if (log->entry_size != expected_size) {
        ESP_LOGE(TAG, "Size mismatch: expected=%d, got=%d", expected_size, log->entry_size);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t expected_crc = calculate_crc8(entry, log->entry_size - 1);
    if (entry[log->entry_size - 1] != expected_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected=0x%02X, got=0x%02X", expected_crc, entry[log->entry_size - 1]);
    }

    if (!get_log_partition()) return ESP_ERR_INVALID_STATE;

    esp_err_t err = esp_partition_read(get_log_partition(), offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t local_offset;
    bool is_slot_getting_full = false;

    bool found = find_next_log_offset(log->entry_size, log->type, &is_slot_getting_full, &local_offset);
    if (!found) {
        ESP_LOGW(TAG, "No space found for log entry.");
        return ESP_ERR_NO_MEM;
    }

    if(is_slot_getting_full) {
        if(write_slot_as_full(offset + local_offset, log->passed_seconds) == ESP_OK) {
            last_saved_passed_duration = log->passed_seconds;
            ESP_LOGI(TAG, "Slot is full written at offset %lu", offset + local_offset);
        }
        else{
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(err));
        }
    }
    else{
        if(write_log_entry(offset + local_offset, entry, log->entry_size) == ESP_OK) {
            last_saved_passed_duration = log->passed_seconds;
            ESP_LOGI(TAG, "Log entry written at offset %lu", offset + local_offset);
        }
        else{
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(err));
        }
    }
    return ESP_OK;
}

//RAM’de bekleyen tüm logları belirtilen flash adresine sırasıyla yazar.
//Örneğin bir terapi tamamlandığında veya belirli koşullar sağlandığında geçici logların hepsi flash'a aktarılır.
static esp_err_t flush_cached_logs_to_slot(uint32_t offset) {
    ESP_LOGI(TAG, "Flush cached logs.");

    for (int i = 0; i < pending_log_count; i++) {
        esp_err_t err = append_log_entry(offset, &pending_logs[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to flush log[%d]: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    pending_log_count = 0;
    return ESP_OK;
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
esp_err_t add_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds) {
    BaseLogEntry log = fill_base_log(type, data, data_len, passed_seconds);
    if (log.entry_size == 0) {
        ESP_LOGE(TAG, "Failed to fill log, skipping add_log");
        return ESP_FAIL;
    }

    if(is_cached_logs_existed) {
        if(log.can_be_cached) {
            ESP_LOGI(TAG, "Log is cached.");
            cache_log_entry(&log);
        }
        if(log.can_flush_cache) {
            is_cached_logs_existed = false;

            uint16_t therapy_count = read_therapy_count();
            ESP_LOGI(TAG, "Therapy count is read as: %u", therapy_count);    
            bool slot_is_finished = false;
            uint32_t base_offset = 0;
    
            if (therapy_count > 0) {
                base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
                ESP_LOGI(TAG, "Base Offset of Slot: %lu", base_offset);
                slot_is_finished = does_slot_contain_entry(base_offset, THERAPY_COMPLETED)
                     || does_slot_contain_entry(base_offset, THERAPY_STOPPED_BY_APP);
                
                if(!slot_is_finished) { //TODO: ve son logdan itibaren 5 dk geçmişse.
                    BaseLogEntry complete_log = fill_base_log(THERAPY_COMPLETED, NULL, 0, 0); //THERAPY_COMPLETED log with zero passed duration indicates that therapy terminated wrong.
                    append_log_entry(base_offset, &complete_log);
                    slot_is_finished = true;
                }

                if (slot_is_finished)
                {
                    ESP_LOGI(TAG, "Slot is completed, new slot is filling.");
    
                    uint16_t new_therapy_count = therapy_count + 1;
                    esp_err_t count_err = write_therapy_count(new_therapy_count);
                    ESP_LOGI(TAG, "New Therapy Count: %u", new_therapy_count);
                    if (count_err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to update therapy counter");
                        return count_err;
                    }
        
                    base_offset = ((new_therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
                    starting_local_offset = 0;

                    ESP_LOGI(TAG, "New Offset: %lu", base_offset);
        
                    //önceden siliyoruz, her zaman en az bir slot boş kalıyor.
                    if (new_therapy_count >= MAX_SAVED_THERAPY) {

                        uint32_t deleting_slot_offset = (new_therapy_count % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
                        esp_err_t erase_err = esp_partition_erase_range(get_log_partition(), deleting_slot_offset, THERAPY_SLOT_SIZE);
                        if (erase_err != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to erase therapy slot before overwriting: %s", esp_err_to_name(erase_err));
                            return erase_err;
                        }
                        ESP_LOGI(TAG, "Deleted slot offset: %lu", deleting_slot_offset);
                    }
                }
            }
    
            else {
                uint16_t new_therapy_count = therapy_count + 1;
                esp_err_t count_err = write_therapy_count(new_therapy_count);
                ESP_LOGI(TAG, "New Therapy Count: %u", new_therapy_count);
                if (count_err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to update therapy counter");
                    return count_err;
                }
    
                base_offset = ((new_therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
                starting_local_offset = 0;
                ESP_LOGI(TAG, "New Base Offset of Slot: %lu", base_offset);
            }
    
            ESP_LOGI(TAG, "Saved cache is flushing to a slot with base offset: %lu", base_offset);

            return flush_cached_logs_to_slot(base_offset);
        }
    }

    else
    {
        if(log.can_be_flashed) {
            uint16_t therapy_count = read_therapy_count();
            if(therapy_count == 0) {
                ESP_LOGE(TAG, "There should be a saved therapy.");
                return ESP_FAIL;
            }
            uint32_t base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
            ESP_LOGI(TAG, "Log is saving the slot of flash with the base offset of: %lu", base_offset);
            return append_log_entry(base_offset, &log);
        }
        
        if(log.can_start_cache) {
            ESP_LOGI(TAG, "Cache is starting.");
            is_cached_logs_existed = true;
            if(log.can_be_cached && !log.can_be_flashed) {
                ESP_LOGI(TAG, "Log is cached.");
                cache_log_entry(&log);
            }
        }
    }

    return ESP_OK;
}


esp_err_t add_notification_log(uint8_t type, uint16_t passed_seconds) {
    uint8_t* data = NULL;
    return add_log(type, data, 0, passed_seconds);
}

esp_err_t read_and_set_records(uint16_t therapy_id) {
    uint32_t base_offset = (therapy_id % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    ESP_LOGI(TAG, "Start to read log of therapy_id: %u", therapy_id);

    esp_err_t err = esp_partition_read(get_log_partition(), base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at index %u: %s", therapy_id, esp_err_to_name(err));
        return err;
    }

    // Her kayıt tipi için bufferlar
    uint8_t* measurements = malloc(THERAPY_SLOT_SIZE);
    uint8_t* notifications = malloc(THERAPY_SLOT_SIZE);
    uint8_t* brightness_updates = malloc(THERAPY_SLOT_SIZE);

    if (!measurements || !notifications || !brightness_updates) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }


    uint16_t max_logs_per_type_measurement = THERAPY_SLOT_SIZE / (get_log_entry_size(MEASUREMENT_CHANGED) - 2);
    uint16_t max_logs_per_type_brightness = THERAPY_SLOT_SIZE / (get_log_entry_size(REGIONS_BRIGHTNESS_UPDATED) - 2);
    uint16_t max_logs_per_type_notification = THERAPY_SLOT_SIZE / (get_log_entry_size(DEVICE_AWAKED) - 1);

    size_t count_measurements = 0;
    size_t count_notifications = 0;
    size_t count_brightness = 0;

    uint16_t therapy_duration = 0;

    uint32_t local_offset = 0;
    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = therapy_slot_buffer[local_offset];
        if (type == 0xFF) break;

        size_t entry_size = get_log_entry_size(type);
        if (entry_size == 0 || local_offset + entry_size > THERAPY_SLOT_SIZE) break;

        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
        const uint8_t* data_ptr = &entry_ptr[1];  // type'ten sonra gelen data
        size_t data_len = entry_size - 4;


        switch (type) {
            case MEASUREMENT_CHANGED:
                if (data_len != 2) break;
                if (count_measurements >= max_logs_per_type_measurement) break;
                memcpy(&measurements[count_measurements * 4], data_ptr, 2); // 2 byte data
                measurements[count_measurements * 4 + 2] = entry_ptr[entry_size - 3]; // passed_seconds (1st byte)
                measurements[count_measurements * 4 + 3] = entry_ptr[entry_size - 2]; // passed_seconds (2nd byte)
                count_measurements++;
                break;

            case REGIONS_BRIGHTNESS_UPDATED:
                if (data_len != 6) break;
                if (count_brightness >= max_logs_per_type_brightness) break;
                memcpy(&brightness_updates[count_brightness * 8], data_ptr, 6);
                brightness_updates[count_brightness * 8 + 6] = entry_ptr[entry_size - 3];
                brightness_updates[count_brightness * 8 + 7] = entry_ptr[entry_size - 2];
                count_brightness++;
                break;
            case THERAPY_STARTED_BY_BUTTON:
            case THERAPY_STARTED_BY_APP:
            {
                if(therapy_duration > 0) {
                    ESP_LOGE(TAG, "Therapy with same therapy id is started more than once.");
                    return ESP_FAIL; 
                }
                if (data_len != 4) break;
                if (therapy_id != ((entry_ptr[entry_size - 7] << 8) | entry_ptr[entry_size - 6])) {
                    ESP_LOGE(TAG, "Wrong therapy id is saved.: %u", ((entry_ptr[entry_size - 7] << 8) | entry_ptr[entry_size - 6]));
                    //return ESP_FAIL; 
                }
                therapy_duration = (entry_ptr[entry_size - 5] << 8) | entry_ptr[entry_size - 4];

                if (count_notifications >= max_logs_per_type_notification) break;
                notifications[count_notifications * 3] = type;
                notifications[count_notifications * 3 + 1] = entry_ptr[entry_size - 2]; // passed_seconds
                notifications[count_notifications * 3 + 2] = entry_ptr[entry_size - 1]; // crc
                count_notifications++;
                break;
            }
            default:
                if (data_len != 0) break;
                if (count_notifications >= max_logs_per_type_notification) break;
                notifications[count_notifications * 3] = type; // hata var mı 
                notifications[count_notifications * 3 + 1] = entry_ptr[entry_size - 2]; // passed_seconds
                notifications[count_notifications * 3 + 2] = entry_ptr[entry_size - 1]; // crc
                count_notifications++;
                break;
        }

        local_offset += entry_size;
    }

    if(therapy_duration == 0) {
        ESP_LOGE(TAG, "Therapy start log is not found.");
        return ESP_FAIL; 
    }
    ESP_LOGI(TAG, "Continue to read log of therapy_id: %u", therapy_id);

    start_encoding_for_new_therapy(therapy_id, therapy_duration, 0);

    if (count_measurements > 0) {
        ESP_LOGI(TAG, "aaaa, count_measurements: %u", count_measurements);
        encode_records_of_therapy(therapy_id, 0x02, 4, count_measurements, measurements);
    }
    if (count_notifications > 0) {
        ESP_LOGI(TAG, "bbbb, count_notifications: %u", count_notifications);
        encode_records_of_therapy(therapy_id, 0x03, 3, count_notifications, notifications);
    }
    if (count_brightness > 0) {
        ESP_LOGI(TAG, "cccc, count_brightness: %u", count_brightness);
        encode_records_of_therapy(therapy_id, 0x04, 8, count_brightness, brightness_updates);
    }
    ESP_LOGI(TAG, "Finish to read log of therapy_id: %u", therapy_id);

    free(measurements);
    free(notifications);
    free(brightness_updates);

    return ESP_OK;
}

/*
//for test
void test_cache_log_limit() {

    BaseLogEntry test_log;
    uint8_t* data = NULL;
    fill_base_log(&test_log, PASSED_DURATION_UPDATED, data, 0, 0);

    for (int i = 0; i < MAX_PENDING_LOGS + 5; ++i) {
        esp_err_t err = cache_log_entry(&test_log);
        ESP_LOGI(TAG, "cache_log_entry() call %d -> %s", i, esp_err_to_name(err));
    }

    print_cached_log_sizes();
}

//for test
void test_flush_to_slot() {

    BaseLogEntry test_log_1;
    uint8_t* data_1 = NULL;
    fill_base_log(&test_log_1, DEVICE_AWAKED, data_1, 0, 29);

    BaseLogEntry test_log_2;
    uint8_t data_2[] = {0x01, 0x2C};
    fill_base_log(&test_log_2, MEASUREMENT_CHANGED, data_2, sizeof(data_2), 123);

    BaseLogEntry test_log_3;
    uint8_t data_3[] = {0x01, 0x2C, 0x01, 0x22};
    fill_base_log(&test_log_3, THERAPY_STARTED_BY_BUTTON, data_3, sizeof(data_3), 200);

    
    cache_log_entry(&test_log_1);

    for (int i = 0; i < 5; ++i) {
        cache_log_entry(&test_log_2);
    }

    cache_log_entry(&test_log_3);

    print_cached_log_sizes();

    flush_cached_logs_to_slot(0);

    print_cached_log_sizes();

    read_logs(0);
}

//for test
void test_add_log_flow() {

    ESP_LOGI(TAG, "DEVICE_AWAKED");
    BaseLogEntry test_log_1;
    uint8_t* data_1 = NULL;
    fill_base_log(&test_log_1, DEVICE_AWAKED, data_1, 0, 9);
    add_log(&test_log_1);

    ESP_LOGI(TAG, "HELMET_ON");

    BaseLogEntry test_log_2;
    uint8_t* data_2 = NULL;
    fill_base_log(&test_log_2, HELMET_ON, data_2, 0, 14);
    add_log(&test_log_2);

    ESP_LOGI(TAG, "THERAPY_STARTED_BY_APP");

    BaseLogEntry start;
    uint8_t data_3[] = {0x01, 0x2C, 0x01, 0x22};
    fill_base_log(&start, THERAPY_STARTED_BY_APP, data_3, sizeof(data_3), 20);
    add_log(&start);

    ESP_LOGI(TAG, "MEASUREMENT_CHANGED");

    BaseLogEntry measure;
    uint8_t data_4[] = {0x01, 0x2C};
    fill_base_log(&measure, MEASUREMENT_CHANGED, data_4, sizeof(data_4), 25);
    add_log(&measure);

    ESP_LOGI(TAG, "THERAPY_COMPLETED");

    BaseLogEntry complete;
    uint8_t* data_5 = NULL;
    fill_base_log(&complete, THERAPY_COMPLETED, data_5, 0, 30);
    add_log(&complete);

    ESP_LOGI(TAG, "DEVICE_AWAKED");

    BaseLogEntry test_log_3;
    uint8_t* data_6 = NULL;
    fill_base_log(&test_log_3, DEVICE_AWAKED, data_6, 0, 40);
    add_log(&test_log_3);

    ESP_LOGI(TAG, "THERAPY_STARTED_BY_APP");

    BaseLogEntry start2;
    uint8_t data_8[] = {0x01, 0x2C, 0x01, 0x22};
    fill_base_log(&start2, THERAPY_STARTED_BY_APP, data_8, sizeof(data_8), 20);
    add_log(&start2);

    ESP_LOGI(TAG, "MEASUREMENT_CHANGED");

    for(int i = 0; i < 1000; i++) {
        ESP_LOGI(TAG, "test_meas:%u", i);

        BaseLogEntry test_log_4;
        uint8_t data_7[] = {0x01, 0x2C};
        fill_base_log(&test_log_4, MEASUREMENT_CHANGED, data_7, sizeof(data_4), 40);
        add_log(&test_log_4);
    }

    ESP_LOGI(TAG, "MEASUREMENT_CHANGED AGAIN");

    BaseLogEntry measure2;
    uint8_t data_9[] = {0x01, 0x2C};
    fill_base_log(&measure2, MEASUREMENT_CHANGED, data_9, sizeof(data_9), 25);
    add_log(&measure2);

    ESP_LOGI(TAG, "THERAPY_COMPLETED");

    BaseLogEntry complete2;
    uint8_t* data_10 = NULL;
    fill_base_log(&complete2, THERAPY_COMPLETED, data_10, 0, 30);
    add_log(&complete2);

    ESP_LOGI(TAG, "MEASUREMENT_CHANGED");

    BaseLogEntry measure3;
    uint8_t data_11[] = {0x01, 0x2C};
    fill_base_log(&measure3, MEASUREMENT_CHANGED, data_11, sizeof(data_11), 25);
    add_log(&measure3);

    ESP_LOGI(TAG, "read_logs(0)");
    read_logs(0);
    ESP_LOGI(TAG, "read_logs(THERAPY_SLOT_SIZE)");
    read_logs(THERAPY_SLOT_SIZE);
    ESP_LOGI(TAG, "read_logs(2 * THERAPY_SLOT_SIZE)");
    read_logs(2 * THERAPY_SLOT_SIZE);
    ESP_LOGI(TAG, "read_logs(3 * THERAPY_SLOT_SIZE)");
    read_logs(3 * THERAPY_SLOT_SIZE);
    ESP_LOGI(TAG, "read_logs(4 * THERAPY_SLOT_SIZE)");
    read_logs(4 * THERAPY_SLOT_SIZE);
    ESP_LOGI(TAG, "read_logs(5 * THERAPY_SLOT_SIZE)");
    read_logs(5 * THERAPY_SLOT_SIZE);
    ESP_LOGI(TAG, "read_logs(6 * THERAPY_SLOT_SIZE)");
    read_logs(6 * THERAPY_SLOT_SIZE);
}
*/
//for test
void erase_therapy_partition(uint32_t offset) {
    esp_err_t erase_err = esp_partition_erase_range(get_log_partition(), offset, THERAPY_SLOT_SIZE);
    if (erase_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase therapy slot before overwriting: %s", esp_err_to_name(erase_err));
    }
    else {
        ESP_LOGI(TAG, "Therapy partition successfully erased.");
    }
}
/*
//for test
void print_cached_log_sizes() {
    ESP_LOGI(TAG, "Pending log count: %d", pending_log_count);

    for (size_t i = 0; i < pending_log_count; ++i) {
        ESP_LOGI(TAG, "  Log[%d] size: %d", i, pending_logs[i].entry_size);
    }
}

uint16_t get_last_saved_passed_duration()
{
    uint16_t therapy_count = read_therapy_count();
    if(therapy_count == 0) {
        ESP_LOGE(TAG, "There should be a saved therapy.");
        return ESP_FAIL;
    }
    uint32_t offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;

    esp_partition_read(get_log_partition(), offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if(last_saved_passed_duration > 0)
    {
        return last_saved_passed_duration;
    }
    uint32_t local_offset = 0;
    uint8_t entry_size = get_log_entry_size(PASSED_DURATION_UPDATED);
    
    while (local_offset + entry_size <= THERAPY_SLOT_SIZE) {
        uint8_t existing_type = therapy_slot_buffer[local_offset];
        if (existing_type == 0xFF) {
            return (therapy_slot_buffer[offset + local_offset - 3] << 8) | therapy_slot_buffer[offset + local_offset - 2];
        }
        size_t existing_entry_size = get_log_entry_size(existing_type);
        if (existing_entry_size == 0 || local_offset + existing_entry_size > THERAPY_SLOT_SIZE) {
            return UINT16_MAX; // Bozulmuş log veya taşma
        }

        local_offset += existing_entry_size;
    }

    return UINT16_MAX;
}
*/

    /*
    if(type == PASSED_DURATION_UPDATED) {
        uint16_t last_saved_duration;

        esp_err_t err = esp_partition_read(get_log_partition(), offset + local_offset - 3, &last_saved_duration, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read last saved duration");
            return err;
        }    
        uint16_t new_duration = ((entry[size - 3] << 8) | entry[size - 2]);
    
        if (new_duration < last_saved_duration + 10) {
            ESP_LOGW(TAG, "There is no need to save time now. (Δt too small)");
            return ESP_ERR_INVALID_STATE;
        }
    }
    */