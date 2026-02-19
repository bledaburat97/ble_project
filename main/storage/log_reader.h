// log_reader.h
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#include "log_types.h"
#include "log_config.h"

#ifdef __cplusplus
extern "C" {
#endif


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

typedef enum {
    LOG_READ_FULL_SLOT = 0,
    LOG_READ_UNTIL_LAST_BLE_CONNECTED = 1,
} LogReadMode;

typedef struct {
    uint8_t slot_buf[THERAPY_SLOT_SIZE];
    bool loaded;
    uint32_t base_offset;
} LogReaderSlot;

void log_reader_slot_reset(LogReaderSlot *s);
esp_err_t log_reader_slot_load(LogReaderSlot *s, uint32_t base_offset);


bool log_reader_read_records(uint16_t therapy_id, ReadTherapyLogs *therapy_logs, LogReadMode mode, const uint8_t *slot_buf);
void log_reader_free_therapy_logs(ReadTherapyLogs *t);

bool log_reader_read_therapy_info(uint16_t therapy_id, ReadTherapyInfo *therapy_info, bool slot_already_loaded, const uint8_t *slot_buf);
uint32_t log_reader_therapy_id_to_base_offset(uint16_t therapy_id);

bool log_reader_slot_buf_contains_type(const uint8_t *slot_buf, uint8_t type_of_entry);
bool log_reader_slot_buf_read_max_passed(const uint8_t *slot_buf, uint16_t *out_max_passed);
bool log_reader_slot_contains_type(uint32_t base_offset, uint8_t type_of_entry);
bool log_reader_read_max_passed(uint32_t base_offset, uint16_t *out_max_passed);


#ifdef __cplusplus
}
#endif
