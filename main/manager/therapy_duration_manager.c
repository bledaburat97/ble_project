#include "therapy_duration_manager.h"

#include "../device_configuration.h"
#include <stdint.h>
#include "esp_log.h"
#include <inttypes.h>
#include <stdbool.h>

static const char *TAG = "TherapyDurationManager";

// Planlanan toplam terapi süresi (s).
static uint16_t planned_therapy_duration_s = 0;
// Terapinin en son duraklamasından önce geçmiş olan terapi süresi
static uint32_t paused_therapy_passed_duration_ms = 0;

static inline uint32_t get_planned_therapy_duration_ms(void)
{
    return (uint32_t)planned_therapy_duration_s * 1000u;
}

static uint32_t clamp_elapsed_to_planned(uint32_t planned_ms, uint64_t candidate_ms)
{
    if (candidate_ms > planned_ms) {
        ESP_LOGW(TAG,"Elapsed duration overflow (candidate=%" PRIu64 " ms, plan=%" PRIu32 " ms) — clamping.", candidate_ms, planned_ms);
        return planned_ms;
    }
    return (uint32_t)candidate_ms;
}

// Planlanan terapi süresini sıfırlar.
void clear_planned_therapy_duration() {
    planned_therapy_duration_s = 0;
}

// Terapi süresini set eder.
bool try_set_planned_therapy_duration(uint16_t duration)
{
    if (duration == 0) {
        ESP_LOGE(TAG, "Attempting to set a zero-duration therapy.");
        return false;
    }

    if (duration > MAX_THERAPY_DURATION) {
        ESP_LOGW(TAG, "Requested therapy duration (%u s) exceeds max (%u s). Clamping.", duration, MAX_THERAPY_DURATION);
        duration = MAX_THERAPY_DURATION;
    }

    ESP_LOGI(TAG, "Current therapy duration is set to %u s.", duration);
    planned_therapy_duration_s = duration;
    return true;
}

// Planlanan terapi süresini döner.
uint16_t get_planned_therapy_duration_s()
{
    return planned_therapy_duration_s;
}

// Terapinin en son duraklamasından önce geçmiş olan terapi süresi
void clear_paused_therapy_passed_duration() {
    paused_therapy_passed_duration_ms = 0;
}

// Terapi duraklatıldığında çağrılır
// Terapinin en son duraklamasından önce geçmiş olan terapi süresini günceller.
void accumulate_paused_therapy_passed_duration(uint32_t therapy_timer_passed_ms, const char *reason)
{
    uint32_t planned_ms   = get_planned_therapy_duration_ms();
    uint64_t combined  = (uint64_t)paused_therapy_passed_duration_ms + (uint64_t)therapy_timer_passed_ms;

    uint32_t new_total = clamp_elapsed_to_planned(planned_ms, combined);
    ESP_LOGI(TAG,
             "%s pause: stored=%" PRIu32 " ms, direct=%" PRIu32 " ms, plan=%" PRIu32 " ms => new=%" PRIu32 " ms",
             reason,
             paused_therapy_passed_duration_ms,
             therapy_timer_passed_ms,
             planned_ms,
             new_total);

    paused_therapy_passed_duration_ms = new_total;
}

// Devam ettirilebilecek pause edilmiş terapi var mı kontrol eder.
bool check_uncompleted_paused_therapy_exists() {
    if(paused_therapy_passed_duration_ms == 0) {
        ESP_LOGW(TAG, "Because there is no passed duration, helmet on action can not start therapy");
        return false;
    }

    if (paused_therapy_passed_duration_ms >= (uint32_t)(planned_therapy_duration_s * 1000)) {
        paused_therapy_passed_duration_ms = (uint32_t)(planned_therapy_duration_s * 1000);
        return false;
    }

    return true;
}

// Pause edilmiş süreyi (s) ms'e çevirip set eder.
void set_paused_therapy_passed_duration_ms(uint16_t therapy_passed_seconds) {
    paused_therapy_passed_duration_ms = (uint32_t)therapy_passed_seconds * 1000;
}

// Pause edilmiş süreyi ms olarak döner.
uint32_t get_paused_therapy_passed_duration_ms() {
    return paused_therapy_passed_duration_ms;
}

// Kalan terapi süresini hesaplar. 
// Bunu hesaplarken hem aktif therapy timer'a bakar hem de bu therapy timer başlamadan önce geçen terapi süresine bakar.
uint16_t get_remaining_therapy_duration() {
    uint32_t plan_ms  = get_planned_therapy_duration_ms();
    if(paused_therapy_passed_duration_ms > plan_ms) {
        ESP_LOGE(TAG, "Paused therapy passed duration exceeded the planned duration");
        return 0;
    }
    uint32_t remain_ms = plan_ms - paused_therapy_passed_duration_ms;
    return (uint16_t)((remain_ms + 999u) / 1000u);
}