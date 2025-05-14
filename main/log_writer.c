#include "log_writer.h"
#include "log_types.h"
#include "log_utils.h"
#include "therapy_counter.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
#include "deep_sleep_manager.h"

#define TAG "LogWriter"

#define MAX_LOG_ENTRY_SIZE 10
#define MAX_PENDING_LOGS 128

static uint8_t pending_logs[MAX_PENDING_LOGS][MAX_LOG_ENTRY_SIZE];
static size_t pending_log_sizes[MAX_PENDING_LOGS];
static size_t pending_log_count = 0;

static bool is_temporary_log_exist = false;
static uint8_t therapy_slot_buffer[THERAPY_SLOT_SIZE];
static uint8_t write_buffer[MAX_LOG_ENTRY_SIZE];


static esp_err_t cache_log_entry(const void* entry, size_t size) {
    if (size > MAX_LOG_ENTRY_SIZE) return ESP_ERR_INVALID_ARG;

    if (pending_log_count >= MAX_PENDING_LOGS) {
        ESP_LOGE(TAG, "Pending log cache FULL. Entering deep sleep...");
        enter_deep_sleep();
        return ESP_ERR_NO_MEM;
    }

    memcpy(pending_logs[pending_log_count], entry, size);
    pending_log_sizes[pending_log_count] = size;
    pending_log_count++;
    return ESP_OK;
}

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


esp_err_t append_log_entry(uint32_t offset, const uint8_t* entry, size_t size) {
    if (!entry || size == 0) return ESP_ERR_INVALID_ARG;

    uint8_t type = entry[0];
    size_t expected_size = get_log_entry_size(type);
    ESP_LOGI(TAG, "append_log_entry: type=%u, expected_size=%u", type, expected_size);

    if (expected_size == 0 || expected_size > MAX_LOG_ENTRY_SIZE) {
        ESP_LOGE(TAG, "Invalid or oversized log type: type=0x%02X", type);
        return ESP_ERR_INVALID_ARG;
    }

    if (size != expected_size) {
        ESP_LOGE(TAG, "Size mismatch: expected=%d, got=%d", expected_size, size);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t expected_crc = calculate_crc8(entry, size - 1);
    if (entry[size - 1] != expected_crc) {
        ESP_LOGW(TAG, "CRC mismatch: expected=0x%02X, got=0x%02X", expected_crc, entry[size - 1]);
    }

    if (!get_log_partition()) return ESP_ERR_INVALID_STATE;

    esp_err_t err = esp_partition_read(get_log_partition(), offset, therapy_slot_buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read therapy slot: %s", esp_err_to_name(err));
        return err;
    }

    uint32_t local_offset = 0;
    bool therapy_gets_completed = false;

    while (local_offset + size <= THERAPY_SLOT_SIZE) {
        if (therapy_slot_buffer[local_offset] == 0xFF) break;

        uint8_t existing_type = therapy_slot_buffer[local_offset];
        if (existing_type == SLOT_IS_FULL) {
            if (type != THERAPY_COMPLETED_LOG) {
                return ESP_ERR_NO_MEM;
            }
            therapy_gets_completed = true;
            break;
        }

        size_t entry_size = get_log_entry_size(existing_type);
        if (entry_size == 0) break;

        local_offset += entry_size;
    }

    ESP_LOGI(TAG, "append_log_entry: local_offset=%lu", local_offset);

    if (!therapy_gets_completed) {
        if (local_offset + size > THERAPY_SLOT_SIZE - 2 * sizeof(Notification_t)) {
            Notification_t slot_full_entry = {
                .type = SLOT_IS_FULL,
                .crc = 0
            };
            const Notification_t* original = (const Notification_t*) entry;
            slot_full_entry.passed_seconds = original->passed_seconds;
            slot_full_entry.crc = calculate_crc8((uint8_t*)&slot_full_entry, sizeof(slot_full_entry) - 1);

            memset(write_buffer, 0, MAX_LOG_ENTRY_SIZE);
            memcpy(write_buffer, &slot_full_entry, sizeof(Notification_t));
            write_buffer[sizeof(Notification_t) - 1] = slot_full_entry.crc;

            err = esp_partition_write(get_log_partition(), offset + local_offset, write_buffer, sizeof(Notification_t));
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write SLOT_IS_FULL entry: %s", esp_err_to_name(err));
                return err;
            }

            ESP_LOGW(TAG, "SLOT_IS_FULL entry written at offset %lu", offset + local_offset);
            return ESP_ERR_NO_MEM;
        }
    } else {
        if (local_offset + size > THERAPY_SLOT_SIZE) {
            ESP_LOGW(TAG, "THERAPY_COMPLETED_LOG can not be saved.");
            return ESP_ERR_NO_MEM;
        }
    }

    memset(write_buffer, 0, MAX_LOG_ENTRY_SIZE);
    memcpy(write_buffer, entry, size);
    write_buffer[size - 1] = calculate_crc8(write_buffer, size - 1);

    err = esp_partition_write(get_log_partition(), offset + local_offset, write_buffer, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write log entry at offset %lu: %s", offset + local_offset, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Log entry written at offset %lu", offset + local_offset);
    return ESP_OK;
}

static esp_err_t flush_cached_logs_to_slot(uint32_t offset) {
    ESP_LOGI(TAG, "Flush cached logs.");

    for (int i = 0; i < pending_log_count; i++) {
        esp_err_t err = append_log_entry(offset, pending_logs[i], pending_log_sizes[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to flush log[%d]: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    pending_log_count = 0;
    return ESP_OK;
}

esp_err_t add_log(const void* entry, size_t size) {
    uint8_t entry_type = ((uint8_t*)entry)[0];

    if (entry_type == THERAPY_STARTED_BY_BUTTON || entry_type == THERAPY_STARTED_BY_APP) {
        is_temporary_log_exist = false;

        uint16_t therapy_count = read_therapy_count();
        ESP_LOGI(TAG, "read_therapy_count: %u", therapy_count);
        cache_log_entry(entry, size);

        bool slot_is_finished = false;
        uint32_t base_offset = 0;

        if (therapy_count > 0) {
            base_offset = ((therapy_count - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
            ESP_LOGI(TAG, "Base Offset: %lu", base_offset);
            slot_is_finished = does_slot_contain_entry(base_offset, THERAPY_COMPLETED_LOG);
            
            if (slot_is_finished) //TODO: ve son logdan itibaren 5 dk geçmişse.
            {
                ESP_LOGI(TAG, "Slot is completed, new slot is filling.");

                uint16_t new_therapy_count = therapy_count + 1;
                esp_err_t count_err = write_therapy_count(new_therapy_count);
                ESP_LOGI(TAG, "New Therapy Count: %u", new_therapy_count);
                if (count_err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to update therapy counter");
                    return count_err;
                }
    
                base_offset = ((new_therapy_count - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    
                ESP_LOGI(TAG, "New Offset: %lu", base_offset);
    
                //önceden siliyoruz, her zaman en az bir slot boş kalıyor.
                if (new_therapy_count >= MAX_SAVED_THERAPY) {
                    uint32_t deleting_slot_offset = (new_therapy_count % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
                    esp_err_t erase_err = esp_partition_erase_range(get_log_partition(), deleting_slot_offset, THERAPY_SLOT_SIZE);
                    if (erase_err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to erase therapy slot before overwriting: %s", esp_err_to_name(erase_err));
                        return erase_err;
                    }
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

            base_offset = ((new_therapy_count - 1)  % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;

            ESP_LOGI(TAG, "New Offset: %lu", base_offset);
        }


        return flush_cached_logs_to_slot(base_offset);
    }

    else if (entry_type == WAKE_FROM_DEEP_SLEEP) {
        is_temporary_log_exist = true;
        ESP_LOGI(TAG, "WAKE_FROM_DEEP_SLEEP");
        return cache_log_entry(entry, size);
    }
    else if (is_temporary_log_exist) {
        ESP_LOGI(TAG, "is_temporary_log_exist");
        return cache_log_entry(entry, size); 
    }

    else {
        if (entry_type == THERAPY_COMPLETED_LOG) is_temporary_log_exist = true;
            
        uint16_t therapy_count = read_therapy_count();
        if(therapy_count == 0) {
            ESP_LOGE(TAG, "There should be a saved therapy.");
            return ESP_FAIL;
        }
        uint32_t base_offset = ((therapy_count - 1) % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
        ESP_LOGI(TAG, "base offset to append_log_entry: %lu", base_offset);
        return append_log_entry(base_offset, entry, size);
    }
    
}

//for test
void test_cache_log_limit() {
    Notification_t test_log = {
        .type = MEASUREMENT_CHANGED,
        .passed_seconds = 0,
        .crc = 0  // doldurulmasa da olur çünkü cache'de CRC yazılmıyor
    };

    for (int i = 0; i < MAX_PENDING_LOGS + 5; ++i) {
        esp_err_t err = cache_log_entry(&test_log, sizeof(Notification_t));
        ESP_LOGI(TAG, "cache_log_entry() call %d -> %s", i, esp_err_to_name(err));
    }

    print_cached_log_sizes();
}

//for test
void test_flush_to_slot() {

    Notification_t test_log_1 = {
        .type = WAKE_FROM_DEEP_SLEEP,
        .passed_seconds = 29,
        .crc = 0
    };
    test_log_1.crc = calculate_crc8((uint8_t*)&test_log_1, sizeof(Notification_t) - 1);

    Measurement_changed_t test_log_2 = {
        .type = MEASUREMENT_CHANGED,
        .passed_seconds = 123,
        .crc = 0
    };
    test_log_2.crc = calculate_crc8((uint8_t*)&test_log_2, sizeof(Measurement_changed_t) - 1);

    Therapy_initialization_t test_log_3 = {
        .type = THERAPY_STARTED_BY_BUTTON,
        .therapy_id = 1,
        .therapy_duration = 10,
        .passed_seconds = 200,
        .crc = 0
    };
    test_log_3.crc = calculate_crc8((uint8_t*)&test_log_3, sizeof(Therapy_initialization_t) - 1);
    
    cache_log_entry(&test_log_1, sizeof(Notification_t));

    for (int i = 0; i < 5; ++i) {
        cache_log_entry(&test_log_2, sizeof(Measurement_changed_t));
    }

    cache_log_entry(&test_log_3, sizeof(Therapy_initialization_t));

    print_cached_log_sizes();

    flush_cached_logs_to_slot(0);

    print_cached_log_sizes();

    read_logs(0);
}

//for test
void test_add_log_flow() {

    Notification_t test_log_1 = {
        .type = WAKE_FROM_DEEP_SLEEP,
        .passed_seconds = 9,
        .crc = 0
    };
    test_log_1.crc = calculate_crc8((uint8_t*)&test_log_1, sizeof(Notification_t) - 1);

    add_log(&test_log_1, sizeof(test_log_1));


    Notification_t test_log_2 = {
        .type = HELMET_ON_LOG,
        .passed_seconds = 14,
        .crc = 0
    };
    test_log_2.crc = calculate_crc8((uint8_t*)&test_log_2, sizeof(Notification_t) - 1);

    add_log(&test_log_2, sizeof(test_log_2));

    Therapy_initialization_t start = {
        .type = THERAPY_STARTED_BY_APP,
        .therapy_id = 1,
        .therapy_duration = 20,
        .passed_seconds = 0,
    };
    start.crc = calculate_crc8((uint8_t*)&start, sizeof(start) - 1);
    add_log(&start, sizeof(start));

    Measurement_changed_t measure = {
        .type = MEASUREMENT_CHANGED,
        .passed_seconds = 5,
    };
    measure.crc = calculate_crc8((uint8_t*)&measure, sizeof(measure) - 1);
    add_log(&measure, sizeof(measure));

    Notification_t complete = {
        .type = THERAPY_COMPLETED_LOG,
        .passed_seconds = 20,
    };
    complete.crc = calculate_crc8((uint8_t*)&complete, sizeof(complete) - 1);
    add_log(&complete, sizeof(complete));

    Notification_t test_log_3 = {
        .type = WAKE_FROM_DEEP_SLEEP,
        .passed_seconds = 9,
        .crc = 0
    };
    test_log_3.crc = calculate_crc8((uint8_t*)&test_log_3, sizeof(Notification_t) - 1);

    add_log(&test_log_3, sizeof(test_log_3));

    read_logs(0);
    read_logs(THERAPY_SLOT_SIZE);
    read_logs(2 * THERAPY_SLOT_SIZE);
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

//for test
void print_cached_log_sizes() {
    ESP_LOGI(TAG, "Pending log count: %d", pending_log_count);

    for (size_t i = 0; i < pending_log_count; ++i) {
        ESP_LOGI(TAG, "  Log[%d] size: %d", i, pending_log_sizes[i]);
    }
}