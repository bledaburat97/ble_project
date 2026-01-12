// log_reader.c
#include "log_reader.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"

#include "log_storage.h"
#include "log_utils.h"      // calculate_crc8, get_log_entry_size_info vb.
#include "therapy_counter.h" // sadece therapy_id->offset hesapta gerekebilir (istersen çıkar)

#define TAG "LogReader"

static LogReaderSlot s_work_slot;

static inline bool verify_crc(const uint8_t *entry_ptr, LogEntrySizeInfo si)
{
    if (!entry_ptr) return false;
    if (si.total_length < 4) return false;
    uint8_t expected = calculate_crc8(entry_ptr, si.total_length - 1);
    uint8_t actual = entry_ptr[si.total_length - 1];
    return expected == actual;
}

static void free_partial(ReadTherapyLogs *t)
{
    if (!t) return;
    free(t->measurements); t->measurements = NULL;
    free(t->notifications); t->notifications = NULL;
    free(t->brightness_updates); t->brightness_updates = NULL;
    t->count_measurements = 0;
    t->count_notifications = 0;
    t->count_brightness = 0;
}

void log_reader_free_therapy_logs(ReadTherapyLogs *t)
{
    free_partial(t);
}

void log_reader_slot_reset(LogReaderSlot *s)
{
    if (!s) return;
    s->loaded = false;
    s->base_offset = 0;
    memset(s->slot_buf, 0xFF, sizeof(s->slot_buf));
}

esp_err_t log_reader_slot_load(LogReaderSlot *s, uint32_t base_offset)
{
    if (!s) return ESP_ERR_INVALID_ARG;
    esp_err_t e = log_storage_read_slot(base_offset, s->slot_buf, sizeof(s->slot_buf));
    if (e == ESP_OK) {
        s->loaded = true;
        s->base_offset = base_offset;
    }
    return e;
}

uint32_t log_reader_therapy_id_to_base_offset(uint16_t therapy_id)
{
    return ((uint32_t)((therapy_id - 1) % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;
}



bool log_reader_read_records(uint16_t therapy_id, ReadTherapyLogs *therapy_logs, LogReadMode mode, const uint8_t *slot_buf)
{
    if (!therapy_logs) return false;
    memset(therapy_logs, 0, sizeof(*therapy_logs));

    therapy_logs->measurements = malloc(MAX_MEASUREMENT_LOGS * 4);
    therapy_logs->notifications = malloc(MAX_NOTIFICATION_LOGS * 3);
    therapy_logs->brightness_updates = malloc(MAX_BRIGHTNESS_LOGS * 8);
    if (!therapy_logs->measurements || !therapy_logs->notifications || !therapy_logs->brightness_updates) {
        ESP_LOGE(TAG, "malloc failed");
        free_partial(therapy_logs);
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
        uint8_t type = slot_buf[local_offset];
        if (type == 0xFF) break;

        LogEntrySizeInfo size_info = get_log_entry_size_info(type);
        if (size_info.total_length == 0 || local_offset + size_info.total_length > THERAPY_SLOT_SIZE) {
            ESP_LOGE(TAG, "Corrupt entry at local=%lu", (unsigned long)local_offset);
            break;
        }

        const uint8_t *entry_ptr = &slot_buf[local_offset];
        if (!verify_crc(entry_ptr, size_info)) {
            ESP_LOGE(TAG, "CRC mismatch at local=%lu", (unsigned long)local_offset);
            break;
        }

        const uint8_t *data_ptr = &entry_ptr[1];
        const uint8_t first_passed_duration_byte_index = 1 + size_info.data_length;

        // Mode cutoff
        if (mode == LOG_READ_UNTIL_FIRST_BLE_CONNECTED && type == BLE_CONNECTED) {
            break;
        }

        switch (type) {
            case MEASUREMENT_CHANGED:
                if (count_measurements < MAX_MEASUREMENT_LOGS) {
                    memcpy(&therapy_logs->measurements[count_measurements * 4], data_ptr, size_info.data_length);
                    therapy_logs->measurements[count_measurements * 4 + size_info.data_length] = entry_ptr[first_passed_duration_byte_index];
                    therapy_logs->measurements[count_measurements * 4 + size_info.data_length + 1] = entry_ptr[first_passed_duration_byte_index + 1];
                    count_measurements++;
                }
                break;

            case NOTIF_BRIGHTNESS_UPDATED:
                if (count_brightness < MAX_BRIGHTNESS_LOGS) {
                    memcpy(&therapy_logs->brightness_updates[count_brightness * 8], data_ptr, 6);
                    therapy_logs->brightness_updates[count_brightness * 8 + size_info.data_length] = entry_ptr[first_passed_duration_byte_index];
                    therapy_logs->brightness_updates[count_brightness * 8 + size_info.data_length + 1] = entry_ptr[first_passed_duration_byte_index + 1];
                    count_brightness++;
                }
                break;

            case PASSED_DURATION_UPDATED:
            case FLASH_SLOT_IS_FULL:
                break;

            default:
                if (count_notifications < MAX_NOTIFICATION_LOGS) {
                    therapy_logs->notifications[count_notifications * 3] = type;
                    therapy_logs->notifications[count_notifications * 3 + 1] = entry_ptr[first_passed_duration_byte_index];
                    therapy_logs->notifications[count_notifications * 3 + 2] = entry_ptr[first_passed_duration_byte_index + 1];
                    count_notifications++;
                }
                break;
        }

        local_offset += size_info.total_length;
    }

    therapy_logs->count_measurements = count_measurements;
    therapy_logs->count_notifications = count_notifications;
    therapy_logs->count_brightness = count_brightness;

    if (count_measurements == 0 && count_notifications == 0 && count_brightness == 0) {
        free_partial(therapy_logs);
        return false;
    }

    return true;
}

bool log_reader_read_therapy_info(uint16_t therapy_id, ReadTherapyInfo *therapy_info, bool slot_already_loaded, const uint8_t *slot_buf)
{
    if (!therapy_info) return false;

    const uint8_t *buf = slot_buf;

    if (!slot_already_loaded) {
        uint32_t base_offset = log_reader_therapy_id_to_base_offset(therapy_id);

        log_storage_lock();
        esp_err_t e = log_storage_read_slot(base_offset, s_work_slot.slot_buf, sizeof(s_work_slot.slot_buf));
        log_storage_unlock();

        if (e != ESP_OK) {
            ESP_LOGE(TAG, "read_therapy_info slot read fail: %s", esp_err_to_name(e));
            return false;
        }
        buf = s_work_slot.slot_buf;
    } else {
        if (!buf) return false;
    }

    memset(therapy_info, 0, sizeof(*therapy_info));
    therapy_info->therapy_duration = 0;
    therapy_info->passed_duration = 0;
    therapy_info->is_over = false;

    uint32_t local_offset = 0;

    while (local_offset < THERAPY_SLOT_SIZE) {
        uint8_t type = buf[local_offset];
        if (type == 0xFF) break;

        LogEntrySizeInfo size_info = get_log_entry_size_info(type);
        if (size_info.total_length == 0 || local_offset + size_info.total_length > THERAPY_SLOT_SIZE) break;

        const uint8_t *entry = &buf[local_offset];
        if (!verify_crc(entry, size_info)) break;

        therapy_info->passed_duration = (entry[size_info.total_length - 3] << 8) | entry[size_info.total_length - 2];

        const uint8_t *data_ptr = &entry[1];

        switch (type) {
            case NOTIF_BRIGHTNESS_UPDATED:
                memcpy(therapy_info->brightness, data_ptr, 6);
                break;

            case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
            case TIMER_STATE_NEW_THERAPY_BY_APP: {
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
            } break;

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

bool log_reader_slot_buf_contains_type(const uint8_t *slot_buf, uint8_t type_of_entry)
{
    if (!slot_buf) return false;

    uint32_t local = 0;
    while (local < THERAPY_SLOT_SIZE) {
        uint8_t t = slot_buf[local];
        if (t == 0xFF) break;
        if (t == type_of_entry) return true;

        LogEntrySizeInfo si = get_log_entry_size_info(t);
        if (si.total_length == 0 || local + si.total_length > THERAPY_SLOT_SIZE) break;

        const uint8_t *entry = &slot_buf[local];
        if (!verify_crc(entry, si)) break;

        local += si.total_length;
    }
    return false;
}

bool log_reader_slot_contains_type(uint32_t base_offset, uint8_t type_of_entry)
{
    uint8_t buf[THERAPY_SLOT_SIZE];

    log_storage_lock();
    esp_err_t e = log_storage_read_slot(base_offset, buf, sizeof(buf));
    log_storage_unlock();

    if (e != ESP_OK) return false;

    return log_reader_slot_buf_contains_type(buf, type_of_entry);
}

bool log_reader_slot_buf_read_max_passed(const uint8_t *buf, uint16_t *out_max_passed)
{
    if (!buf || !out_max_passed) return false;

    uint32_t local = 0;
    uint16_t maxp = 0;
    bool any = false;

    while (local < THERAPY_SLOT_SIZE) {
        uint8_t t = buf[local];
        if (t == 0xFF) break;

        LogEntrySizeInfo si = get_log_entry_size_info(t);
        if (si.total_length == 0 || local + si.total_length > THERAPY_SLOT_SIZE) break;

        const uint8_t *entry = &buf[local];
        if (!verify_crc(entry, si)) break;

        uint16_t passed = (entry[si.total_length - 3] << 8) | entry[si.total_length - 2];
        if (!any || passed > maxp) { maxp = passed; any = true; }

        local += si.total_length;
    }

    if (any) *out_max_passed = maxp;
    return any;
}

bool log_reader_read_max_passed(uint32_t base_offset, uint16_t *out_max_passed)
{
    if (!out_max_passed) return false;

    uint8_t buf[THERAPY_SLOT_SIZE];

    log_storage_lock();
    esp_err_t e = log_storage_read_slot(base_offset, buf, sizeof(buf));
    log_storage_unlock();

    if (e != ESP_OK) {
        ESP_LOGE(TAG, "read_max_passed read fail: %s", esp_err_to_name(e));
        return false;
    }

    return log_reader_slot_buf_read_max_passed(buf, out_max_passed);
}

