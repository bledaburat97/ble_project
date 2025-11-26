#pragma once

#include "storage/log_types.h"
#ifndef UNIT_TESTING
#include "esp_err.h"
#else
#include "fake_esp_err.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EXISTS = 0,
    DONE = 1,
} TherapyState;

typedef struct {
    uint8_t* measurements;      // malloc edilmiş buffer
    uint8_t* notifications;     // malloc edilmiş buffer
    uint8_t* brightness_updates;// malloc edilmiş buffer
    size_t count_measurements;
    size_t count_notifications;
    size_t count_brightness;
} ReadTherapyLogs;

typedef struct {
    bool is_over;
    uint8_t brightness[6];
    uint16_t therapy_duration;
    uint16_t passed_duration;
} ReadTherapyInfo;


esp_err_t add_log(uint8_t type, const uint8_t* data, size_t data_len, uint16_t passed_seconds);
esp_err_t add_notification_log(uint8_t type, uint16_t passed_seconds);
void print_cached_log_sizes();
void erase_therapy_partition(uint32_t offset);
bool read_records(uint16_t therapy_id, ReadTherapyLogs* therapy_logs, bool is_active_therapy);
bool read_therapy_info(uint16_t therapy_id, ReadTherapyInfo* therapy_info);
void read_and_print_test_logs(uint8_t therapy_id);
#ifdef __cplusplus
}
#endif
