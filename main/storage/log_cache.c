#include "log_cache.h"
#include "esp_log.h"

#define TAG "LogCache"

// Cache: terapi başlamadan önce gelen logları RAM'de tutar.
// Terapi kesinleşince bu kayıtlar ilgili slota taşınır.
// Amaç: erken olayları kaybetmemek ve flash yazımını gereksiz büyütmemek. 
// Ana mantık terapi başlamadığı zaman tutulacak logların önemsiz olması.
void log_cache_init(LogCache *c)
{
    if (!c) return;
    c->count = 0;
    c->active = false;
}

void log_cache_start(LogCache *c)
{
    if (!c) return;
    c->active = true;
}

void log_cache_stop(LogCache *c)
{
    if (!c) return;
    c->active = false;
}

bool log_cache_is_active(const LogCache *c)
{
    return c && c->active;
}

size_t log_cache_count(const LogCache *c)
{
    return c ? c->count : 0;
}

esp_err_t log_cache_push(LogCache *c, const BaseLogEntry *e)
{
    if (!c || !e) return ESP_ERR_INVALID_ARG;

    if (c->count >= MAX_PENDING_LOGS) {
        ESP_LOGW(TAG, "Cache full (%u). Clearing cache and continuing.", (unsigned)c->count);
        log_cache_clear(c);
    }

    c->entries[c->count++] = *e;
    return ESP_OK;
}

void log_cache_clear(LogCache *c)
{
    if (!c) return;
    c->count = 0;
}
