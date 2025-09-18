#include "log_writer.h"
#include "log_types.h"
#include "log_utils.h"
#include "log_partition_manager.h"
#include "therapy_counter.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "deep_sleep_manager.h"
#include "matching_message_encoder.h"

#define TAG "LogWriter"

#define THERAPY_SLOT_SIZE 4096
#define MAX_SAVED_THERAPY 500
#define MAX_PENDING_LOGS 128

static BaseLogEntry pending_logs[MAX_PENDING_LOGS];
static size_t pending_log_count = 0;

static bool is_cached_logs_existed = false;
static uint8_t therapy_slot_buffer[THERAPY_SLOT_SIZE];
static uint8_t write_buffer[MAX_LOG_ENTRY_SIZE];

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
    else if (data_len == 0) {
        memset(&out_entry[1], 0, MAX_LOG_ENTRY_SIZE - 4);
    }
    else{
        ESP_LOGE(TAG, "data_len can not be smaller than zero.");
        return false;
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
    //ESP_LOGI(TAG, "Appending log entry to offset: %lu", offset);
    uint8_t entry[MAX_LOG_ENTRY_SIZE] = {0};
    if (!create_log_entry(log, entry)) {
        ESP_LOGE(TAG, "Failed to create log entry from BaseLogEntry");
        return ESP_ERR_INVALID_ARG;
    }

    size_t expected_size = get_log_entry_size_info(log->type).total_length;
    //ESP_LOGI(TAG, "Saving log with type:%u and expected size:%u", log->type, expected_size);

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
        //ESP_LOGW(TAG, "No space found for log entry.");
        return ESP_ERR_NO_MEM;
    }

    if(is_slot_getting_full) {
        if(write_slot_as_full(offset + local_offset, log->passed_seconds) == ESP_OK) {
            ESP_LOGI(TAG, "Slot is full written at offset %lu", offset + local_offset);
            starting_local_offset = local_offset + sizeof(Notification_t);
        }
        else{
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(err));
            return ESP_FAIL;
        }
    }
    else{
        if(write_log_entry(offset + local_offset, entry, log->entry_size) == ESP_OK) {
            ESP_LOGI(TAG, "Log entry written at offset %lu", offset + local_offset);
            starting_local_offset = local_offset + log->entry_size;
        }
        else{
            ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(err));
            return ESP_FAIL;
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

        size_t size = get_log_entry_size_info(type).total_length;
        if (size == 0 || local_offset + size > THERAPY_SLOT_SIZE) break;

        local_offset += size;
    }

    return false;
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
    ESP_LOGI(TAG, "Adding log type: %u", log.type);

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
                slot_is_finished = does_slot_contain_entry(base_offset, NOTIF_THERAPY_COMPLETED) 
                        || does_slot_contain_entry(base_offset, NOTIF_THERAPY_STOPPED_BY_APP) 
                        || does_slot_contain_entry(base_offset, NOTIF_SHUT_DOWN_BY_BUTTON);
                
                if(!slot_is_finished) { //TODO: ve son logdan itibaren 5 dk geçmişse.
                    BaseLogEntry complete_log = fill_base_log(NOTIF_THERAPY_COMPLETED, NULL, 0, 0); //THERAPY_COMPLETED log with zero passed duration indicates that therapy terminated wrong.
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
            //ESP_LOGI(TAG, "Log is saving the slot of flash with the base offset of: %lu", base_offset);
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
    ESP_LOGE(TAG, "Log of notification type of %u is being added with passed_seconds: %u", type, passed_seconds);
    uint8_t* data = NULL;
    return add_log(type, data, 0, passed_seconds);
}

//for test
void test_add_log_flow() {

    uint8_t* data_1 = NULL;
    add_log(DEVICE_AWAKED, data_1, 0, 2);

    uint8_t* data_2 = NULL;
    add_log(NOTIF_HELMET_ON, data_2, 0, 4);

    uint8_t data_3[] = {0x64, 0x50};
    add_log(MEASUREMENT_CHANGED, data_3, sizeof(data_3), 6);

    /*
    uint8_t* data_4 = NULL;
    add_log(DEVICE_INFO_MESSAGE_ACK, data_4, 0, 9);
    */

    uint8_t data_5[] = {0x00, 0x05, 0x04, 0xB0};
    add_log(TIMER_STATE_NEW_THERAPY_BY_APP, data_5, sizeof(data_5), 20);

    uint8_t data_6[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0xFF};
    add_log(NOTIF_BRIGHTNESS_UPDATED, data_6, sizeof(data_6), 24);

    uint8_t data_7[] = {0x68, 0x4B};
    add_log(MEASUREMENT_CHANGED, data_7, sizeof(data_7), 26);

    uint8_t* data_8 = NULL;
    add_log(NOTIF_HELMET_ON, data_8, 0, 28);

    uint8_t* data_9 = NULL;
    add_log(NOTIF_HELMET_ON, data_9, 0, 34);

    uint8_t* data_10 = NULL;
    add_log(NOTIF_HELMET_ON, data_10, 0, 260);

    uint8_t data_11[] = {0x6C, 0x46};
    add_log(MEASUREMENT_CHANGED, data_11, sizeof(data_11), 268);

    uint8_t data_12[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
    add_log(NOTIF_BRIGHTNESS_UPDATED, data_12, sizeof(data_12), 270);

    uint8_t* data_13 = NULL;
    add_log(NOTIF_THERAPY_COMPLETED, data_13, 0, 280);
}

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
*/

bool read_records(uint16_t therapy_id, ReadTherapyLogs* therapy_logs, bool is_active_therapy) {
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

    therapy_logs->measurements = malloc(THERAPY_SLOT_SIZE);
    therapy_logs->notifications = malloc(THERAPY_SLOT_SIZE);
    therapy_logs->brightness_updates = malloc(THERAPY_SLOT_SIZE);

    if (!therapy_logs->measurements || !therapy_logs->notifications || !therapy_logs->brightness_updates) {
        ESP_LOGE(TAG, "Memory allocation failed");
        free(therapy_logs->measurements);
        free(therapy_logs->notifications);
        free(therapy_logs->brightness_updates);
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

        const uint8_t first_data_byte_index = 1;
        const uint8_t first_passed_duration_byte_index = first_data_byte_index + size_info.data_length;
        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
        const uint8_t* data_ptr = &entry_ptr[first_data_byte_index];  // type'ten sonra gelen data

        switch (type) {
            case MEASUREMENT_CHANGED:
                if (therapy_logs->count_measurements >= MAX_MEASUREMENT_LOGS) break;

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

            case BLE_CONNECTED:
                if (count_notifications >= MAX_NOTIFICATION_LOGS) break;

                therapy_logs->notifications[count_notifications * 3] = type; // hata var mı 
                therapy_logs->notifications[count_notifications * 3 + 1] = entry_ptr[first_passed_duration_byte_index]; // passed_seconds
                therapy_logs->notifications[count_notifications * 3 + 2] = entry_ptr[first_passed_duration_byte_index + 1]; // passed_seconds
                count_notifications++;

                if (is_active_therapy) {
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

    if (!is_active_therapy) {
        therapy_logs->count_notifications = count_notifications;
        therapy_logs->count_brightness = count_brightness;
        therapy_logs->count_measurements = count_measurements;
    }

    if (therapy_logs->count_notifications == 0 && therapy_logs->count_brightness == 0 && therapy_logs->count_measurements == 0) {
        free(therapy_logs->measurements);
        free(therapy_logs->notifications);
        free(therapy_logs->brightness_updates);
        return false;
    }
    ESP_LOGI(TAG, "Read records for therapy id: %u is successful.", therapy_id);
    return true;

}

bool read_therapy_info(uint16_t therapy_id, ReadTherapyInfo* therapy_info) {
    uint32_t base_offset = ((therapy_id - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    const esp_partition_t* partition = get_log_partition();
    if (!partition) {
        ESP_LOGE(TAG, "Cannot read flash, partition is NULL!");
        return false;
    }

    esp_err_t err = esp_partition_read(partition, base_offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read slot at index %u: %s", therapy_id, esp_err_to_name(err));
        return false;
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

        therapy_info->passed_duration = (therapy_slot_buffer[local_offset + size_info.total_length - 3] << 8 ) | therapy_slot_buffer[local_offset + size_info.total_length - 2]; 

        const uint8_t first_data_byte_index = 1;
        const uint8_t* entry_ptr = &therapy_slot_buffer[local_offset];
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
                //TODO: değiştirilen kodu kontrol et.
                /*
                if (therapy_id != ((entry_ptr[size_info.total_length - 7] << 8) | entry_ptr[size_info.total_length - 6])) {
                    ESP_LOGE(TAG, "Wrong therapy id is saved.: %u", ((entry_ptr[size_info.total_length - 7] << 8) | entry_ptr[size_info.total_length - 6]));
                    return false;
                }
                therapy_info->therapy_duration = (entry_ptr[size_info.total_length - 5] << 8) | entry_ptr[size_info.total_length - 4];
                */
                uint16_t t_id = (data_ptr[0] << 8) | data_ptr[1];
                uint16_t t_dur = (data_ptr[2] << 8) | data_ptr[3];
                if (therapy_id != t_id) {
                    ESP_LOGE(TAG, "Wrong therapy id is saved.: %u", t_id);
                    return false;
                }
                therapy_info->therapy_duration = t_dur;
                
                break;
            }
            case NOTIF_SHUT_DOWN_BY_BUTTON:
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




