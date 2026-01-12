// log_resume.h
#pragma once

#include <stdbool.h>
#include "log_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t therapy_id;
    uint16_t therapy_duration;
    uint16_t last_passed_seconds;
    uint16_t therapy_passed_seconds;
    uint8_t  last_brightness[6];
} UncompletedTherapyInfo;

/**
 * Uncompleted therapy info çıkarma.
 * Orchestrator/dev_init gibi yerler burayı çağırır.
 */
bool log_resume_read_uncompleted_therapy(UncompletedTherapyInfo *out);

#ifdef __cplusplus
}
#endif
