#include "log_cache.h"
#include "esp_log.h"

#define TAG "LogCache"

void log_cache_init(LogCache *c)
{
    if (!c) return;
    c->count = 0;
    c->active = false;
    c->disabled = false;
}

void log_cache_start(LogCache *c)
{
    if (!c || c->disabled) return;
    c->active = true;
}

void log_cache_stop(LogCache *c)
{
    if (!c) return;
    c->active = false;
}

bool log_cache_is_active(const LogCache *c)
{
    return c && c->active && !c->disabled;
}

size_t log_cache_count(const LogCache *c)
{
    return c ? c->count : 0;
}

bool log_cache_is_disabled(const LogCache *c)
{
    return c ? c->disabled : true;
}

void log_cache_disable(LogCache *c)
{
    if (!c) return;
    c->disabled = true;
    c->active = false;
    c->count = 0;
}

esp_err_t log_cache_push(LogCache *c, const BaseLogEntry *e)
{
    if (!c || !e) return ESP_ERR_INVALID_ARG;
    if (c->disabled) return ESP_OK;

    if (c->count >= MAX_PENDING_LOGS) {
        // Drop newest + disable (isteğe göre sadece drop da yapabilirsin)
        ESP_LOGE(TAG, "Cache full (%u). Disabling logging cache and dropping.", (unsigned)c->count);
        log_cache_disable(c);
        return ESP_ERR_NO_MEM;
    }

    c->entries[c->count++] = *e;
    return ESP_OK;
}

void log_cache_clear(LogCache *c)
{
    if (!c) return;
    c->count = 0;
}