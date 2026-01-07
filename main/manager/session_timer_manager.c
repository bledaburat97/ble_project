#include "session_timer_manager.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SessionTimerManager";

static int64_t session_start_us = -1; // -1: aktif oturum yok
static inline bool is_session_running(void) { return session_start_us >= 0; }
static int64_t passed_duration_before_deep_sleep = 0;


uint16_t get_session_passed_seconds(void) {
    if (!is_session_running()) {
        ESP_LOGI(TAG, "Session is not running currently.");
        return 0;
    }

    int64_t now_us = esp_timer_get_time();

    int64_t diff = now_us - session_start_us + passed_duration_before_deep_sleep;

    if (diff < 0) {
        diff = 0;
    }

    const int64_t max_diff_us = (int64_t)UINT16_MAX * 1000000LL;
    if (diff > max_diff_us) {
        ESP_LOGE(TAG, "Session diff overflow (>%u s). Clamping.", UINT16_MAX);
        return UINT16_MAX;
    }

    uint64_t seconds = (uint64_t)diff / 1000000ULL;
    return (uint16_t)seconds;
}

void clear_session_clock(void) {
    ESP_LOGW(TAG, "Session cleared");
    session_start_us = -1;
}

void reset_session_clock(int64_t passed_time) {
    passed_duration_before_deep_sleep = passed_time;
    session_start_us = esp_timer_get_time();
}
