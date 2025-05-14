#include "log_parser.h"
#include "log_utils.h"
#include "log_types.h"
#include "esp_log.h"
#include <string.h>

#define TAG "LogParser"

esp_err_t parse_therapy_session(uint16_t therapy_id, TherapySession* session) {
    memset(session, 0, sizeof(TherapySession));
    session->therapy_id = therapy_id;
    session->duration = 0;
    uint32_t offset = (therapy_id % MAX_SAVED_THERAPY) * THERAPY_SLOT_SIZE;
    uint8_t buffer[THERAPY_SLOT_SIZE];
    esp_err_t err = esp_partition_read(get_log_partition(), offset, buffer, THERAPY_SLOT_SIZE);
    if (err != ESP_OK) return err;

    uint32_t pos = 0;
    Time_saved_t* last_time_saved = NULL;
    uint16_t passed_therapy_duration = 0;
    uint16_t last_instant_of_starting_therapy = 0;
    uint16_t last_log_instant = 0;
    while (pos < THERAPY_SLOT_SIZE) {
        uint8_t type = buffer[pos];
        if (type == 0xFF) break;

        size_t size = get_log_entry_size(type);
        if (size == 0 || pos + size > THERAPY_SLOT_SIZE) break;

        const uint8_t* data = &buffer[pos];

        if (calculate_crc8(data, size - 1) != data[size - 1]) {
            ESP_LOGE(TAG, "CRC check failed at offset %lu", pos);
            break;
        }
        switch (type) {
            case THERAPY_TIMER_STOPPED: {
                const Notification_t* n = (const Notification_t*)data;
                last_log_instant = n->passed_seconds;
                passed_therapy_duration += n->passed_seconds - last_instant_of_starting_therapy;
                last_instant_of_starting_therapy = UINT16_MAX;

                if (session->notif_log_count < MAX_NOTIFICATION_LOGS) {
                    NotificationLog* log = &session->notif_logs[session->notif_log_count++];
                    log->type = type;
                    log->time = n->passed_seconds;
                }
                break;
            }

            case THERAPY_CONTINUED_BY_APP:
            case THERAPY_CONTINUED_BY_BUTTON: {
                const Therapy_initialization_t* t = (const Therapy_initialization_t*)data;
                if (t->therapy_id != therapy_id) return ESP_ERR_INVALID_ARG;
                if (session->duration == 0) {
                    ESP_LOGE(TAG, "Therapy was not started.");
                    return ESP_ERR_INVALID_ARG;
                }

                if (session->duration != t-> therapy_duration + passed_therapy_duration) {
                    ESP_LOGE(TAG, "Wrong therapy_duration: %u", t-> therapy_duration);
                    return ESP_ERR_INVALID_ARG;
                }

                last_log_instant = t->passed_seconds;
                last_instant_of_starting_therapy = t->passed_seconds;
                if (session->notif_log_count < MAX_NOTIFICATION_LOGS) {
                    NotificationLog* n = &session->notif_logs[session->notif_log_count++];
                    n->type = type;
                    n->time = t->passed_seconds;
                }
                break;
            }
            case THERAPY_STARTED_BY_APP:
            case THERAPY_STARTED_BY_BUTTON: {
                const Therapy_initialization_t* t = (const Therapy_initialization_t*)data;
                if (t->therapy_id != therapy_id) return ESP_ERR_INVALID_ARG;
                last_log_instant = t->passed_seconds;
                session->duration = t->therapy_duration;
                last_instant_of_starting_therapy = t->passed_seconds;
                if (session->notif_log_count < MAX_NOTIFICATION_LOGS) {
                    NotificationLog* n = &session->notif_logs[session->notif_log_count++];
                    n->type = type;
                    n->time = t->passed_seconds;
                }
                break;
            }
            case MEASUREMENT_CHANGED:{
                const Measurement_changed_t* m = (const Measurement_changed_t*)data;
                last_log_instant = m->passed_seconds;
                if (session->measurement_log_count < MAX_MEASUREMENT_LOGS) {
                    MeasurementChangeLog* t = &session->measurement_logs[session->measurement_log_count++];
                    t->temperature = m->temperature;
                    t->humidity = m->humidity;
                    t->time = m->passed_seconds;
                }
                break;
            }
            case REGIONS_BRIGHTNESS_UPDATED: {
                const Regions_updated_t* r = (const Regions_updated_t*)data;
                last_log_instant = r->passed_seconds;
                if (session->brightness_log_count < MAX_BRIGHTNESS_LOGS) {
                    BrightnessChangeLog* b = &session->brightness_logs[session->brightness_log_count++];
                    memcpy(b->brightness, r->region_brightnesses, 6);
                    b->time = r->passed_seconds;
                }
                break;
            }
            case RTC_TIME_SAVED: {
                last_time_saved = (Time_saved_t*)data;
                last_log_instant = last_time_saved->passed_seconds;
                break;
            }
            default: { // Diğer tüm type’lar Notification_t olarak kabul edilir
                const Notification_t* n = (const Notification_t*)data;
                last_log_instant = n->passed_seconds;
                if (session->notif_log_count < MAX_NOTIFICATION_LOGS) {
                    NotificationLog* log = &session->notif_logs[session->notif_log_count++];
                    log->type = type;
                    log->time = n->passed_seconds;
                }
                break;
            }
        }

        pos += size;
    }

    // Start time hesapla
    if (last_time_saved) {
        uint64_t timestamp = 0;
        for (int i = 0; i < 5; i++) {
            timestamp = (timestamp << 8) | last_time_saved->timestamp[i];
        }
        timestamp -= last_time_saved->passed_seconds;
        for (int i = 0; i < 5; i++) {
            session->start_time[4 - i] = (timestamp >> (i * 8)) & 0xFF;
        }
    }

    // TODO: MAC adresini al
    //esp_read_mac(session->device_id, ESP_MAC_WIFI_STA); // ya da BLE MAC, projen ne kullanıyorsa


    if (last_instant_of_starting_therapy != UINT16_MAX) {
        passed_therapy_duration += last_log_instant - last_instant_of_starting_therapy;
        if (session->notif_log_count < MAX_NOTIFICATION_LOGS) {
            NotificationLog* log = &session->notif_logs[session->notif_log_count++];
            log->type = POWER_IS_OFF;
            log->time = last_log_instant;
        }
    }

    return ESP_OK;
}