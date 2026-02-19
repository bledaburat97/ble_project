// log_cache.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#include "log_types.h"
#include "log_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BaseLogEntry entries[MAX_PENDING_LOGS];
    size_t count;
    bool active;
} LogCache;

void log_cache_init(LogCache *c);
void log_cache_start(LogCache *c);
void log_cache_stop(LogCache *c);
bool log_cache_is_active(const LogCache *c);
size_t log_cache_count(const LogCache *c);

esp_err_t log_cache_push(LogCache *c, const BaseLogEntry *e);
void log_cache_clear(LogCache *c);

#ifdef __cplusplus
}
#endif
