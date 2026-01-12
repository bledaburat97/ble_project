// log_resume.c
#include "log_resume.h"

#include <string.h>
#include "esp_log.h"

#include "log_storage.h"
#include "log_reader.h"
#include "log_utils.h"
#include "therapy_counter.h"
#include "log_config.h"

#define TAG "LogResume"

static uint8_t s_resume_slot_buf[THERAPY_SLOT_SIZE];

static inline bool verify_crc(const uint8_t *entry_ptr, LogEntrySizeInfo si)
{
    if (!entry_ptr) return false;
    if (si.total_length < 4) return false;
    uint8_t expected = calculate_crc8(entry_ptr, si.total_length - 1);
    uint8_t actual = entry_ptr[si.total_length - 1];
    return expected == actual;
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

static bool is_start_or_continue_state(uint8_t type) {
    return (type == TIMER_STATE_NEW_THERAPY_BY_BUTTON) ||
           (type == TIMER_STATE_NEW_THERAPY_BY_APP) ||
           (type == TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON) ||
           (type == TIMER_STATE_CONTINUE_THERAPY_BY_APP);
}

static uint32_t base_offset_for_last(uint16_t therapy_count)
{
    return ((uint32_t)((therapy_count - 1) % MAX_SAVED_THERAPY)) * THERAPY_SLOT_SIZE;
}

static bool compute_uncompleted_passed(const uint8_t *buf, uint16_t *out_last_session_passed, uint16_t *out_therapy_passed)
{

    if (!buf || !out_last_session_passed || !out_therapy_passed) return false;

    uint32_t local = 0;
    bool any = false;

    uint16_t last_session_passed = 0;

    bool last_start_found = false;
    uint16_t last_start_session = 0;
    uint16_t last_start_therapy_passed = 0;

    bool last_pause_found = false;
    uint16_t last_pause_session = 0;

    while (local < THERAPY_SLOT_SIZE) {
        uint8_t type = buf[local];
        if (type == 0xFF) break;

        LogEntrySizeInfo si = get_log_entry_size_info(type);
        if (si.total_length == 0 || local + si.total_length > THERAPY_SLOT_SIZE) break;

        const uint8_t *entry = &buf[local];
        if (!verify_crc(entry, si)) break;

        uint16_t passed = (entry[si.total_length - 3] << 8) | entry[si.total_length - 2];
        last_session_passed = passed;
        any = true;

        if (is_start_or_continue_state(type)) {
            const uint8_t *data = &entry[1];
            uint16_t therapy_passed = 0;

            // data: [therapy_id(2), duration(2), therapy_passed(2)] en az 6 byte
            if (si.data_length >= 6) {
                therapy_passed = (data[4] << 8) | data[5];
            }
            last_start_found = true;
            last_start_session = passed;
            last_start_therapy_passed = therapy_passed;
        } else if (is_pause_state_type(type)) {
            last_pause_found = true;
            last_pause_session = passed;
        }

        local += si.total_length;
    }

    if (!any) return false;

    uint16_t therapy_passed = 0;
    if (last_start_found) {
        if (last_pause_found && last_pause_session >= last_start_session) {
            therapy_passed = last_start_therapy_passed + (last_pause_session - last_start_session);
        } else {
            therapy_passed = last_start_therapy_passed + (last_session_passed - last_start_session);
        }
    } else {
        therapy_passed = 0;
    }

    *out_last_session_passed = last_session_passed;
    *out_therapy_passed = therapy_passed;
    return true;
}

bool log_resume_read_uncompleted_therapy(UncompletedTherapyInfo *out)
{
    if (!out) return false;

    uint16_t therapy_count = read_therapy_count();
    if (therapy_count == 0) return false;

    uint32_t base_offset = log_reader_therapy_id_to_base_offset(therapy_count);

    log_storage_lock();
    esp_err_t e = log_storage_read_slot(base_offset, s_resume_slot_buf, sizeof(s_resume_slot_buf));
    log_storage_unlock();
    if (e != ESP_OK) return false;

    bool finished =
        log_reader_slot_buf_contains_type(s_resume_slot_buf, NOTIF_THERAPY_COMPLETED) ||
        log_reader_slot_buf_contains_type(s_resume_slot_buf, NOTIF_THERAPY_STOPPED_BY_APP) ||
        log_reader_slot_buf_contains_type(s_resume_slot_buf, NOTIF_ENTER_DEEP_SLEEP);

    if (finished) return false;

    ReadTherapyInfo info = {0};
    if (!log_reader_read_therapy_info(therapy_count, &info, true, s_resume_slot_buf)) return false;

    uint16_t last_session=0, therapy_passed=0;
    if (!compute_uncompleted_passed(s_resume_slot_buf, &last_session, &therapy_passed)) {
        last_session = info.passed_duration;
        therapy_passed = info.passed_duration;
    }

    memset(out, 0, sizeof(*out));
    out->therapy_id = therapy_count;
    out->therapy_duration = info.therapy_duration;
    out->last_passed_seconds = last_session;
    out->therapy_passed_seconds = therapy_passed;
    memcpy(out->last_brightness, info.brightness, 6);

    ESP_LOGI(TAG, "Uncompleted therapy: id=%u dur=%u last_session=%u therapy_passed=%u",
             out->therapy_id, out->therapy_duration, out->last_passed_seconds, out->therapy_passed_seconds);
    return true;
}
