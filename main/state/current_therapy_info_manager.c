#include "current_therapy_info_manager.h"

#include "timer_manager.h"

#include "../storage/therapy_counter.h"
#include "../storage/log_writer.h"

#include "../transaction/default_configuration_handler.h"

#include "../i2c/laser/laser_driver_controller.h"

#include "../device_configuration.h"

#include <stdint.h>
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "CurrentTherapyInfoManager";

static CurrentTherapyState s_current_therapy_state = NONE;
static uint16_t s_current_therapy_duration_s = 0;
static uint32_t s_therapy_passed_ms_before_last_pause = 0;

static inline uint32_t get_plan_duration_ms(void)
{
    return (uint32_t)s_current_therapy_duration_s * 1000u;
}

/**
 * Ensures elapsed therapy time does not exceed the planned duration.
 * If candidate_ms > plan_ms, clamps to plan_ms and logs a warning.
 */
static uint32_t clamp_elapsed_to_plan(uint32_t plan_ms, uint64_t candidate_ms)
{
    if (candidate_ms > plan_ms) {
        ESP_LOGW(TAG,
                 "Elapsed duration overflow (candidate=%" PRIu64 " ms, plan=%" PRIu32 " ms) — clamping.",
                 candidate_ms, plan_ms);
        return plan_ms;
    }
    return (uint32_t)candidate_ms;
}

/**
 * Aggregates "already passed" duration with the current timer value,
 * clamps to planned duration and stores result in s_therapy_passed_ms_before_last_pause.
 */
static void accumulate_passed_duration(const char *reason)
{
    uint32_t plan_ms   = get_plan_duration_ms();
    uint32_t direct_ms = get_therapy_passed_ms_direct();
    uint64_t combined  = (uint64_t)s_therapy_passed_ms_before_last_pause + (uint64_t)direct_ms;

    uint32_t new_total = clamp_elapsed_to_plan(plan_ms, combined);
    ESP_LOGI(TAG,
             "%s pause: stored=%" PRIu32 " ms, direct=%" PRIu32 " ms, plan=%" PRIu32 " ms => new=%" PRIu32 " ms",
             reason,
             s_therapy_passed_ms_before_last_pause,
             direct_ms,
             plan_ms,
             new_total);

    s_therapy_passed_ms_before_last_pause = new_total;
}

/**
 * Clears all state related to the current therapy session and resets the session clock.
 */
static void clear_current_therapy(void)
{
    ESP_LOGI(TAG, "Clearing current therapy state.");
    s_current_therapy_duration_s = 0;
    s_current_therapy_state = NONE;
    s_therapy_passed_ms_before_last_pause = 0;
    clear_session_clock();
}

/**
 * Restores an uncompleted therapy session from flash (if available).
 * Returns true if a paused therapy has been restored, false otherwise.
 */
bool restore_uncompleted_therapy_if_exists(void)
{
    UncompletedTherapyInfo info;
    if (!read_uncompleted_therapy(&info)) {
        ESP_LOGI(TAG, "No uncompleted therapy found in flash.");
        return false;
    }

    ESP_LOGI(TAG,
             "Restoring uncompleted therapy: id=%u, duration=%u s, last_passed=%u s",
             info.therapy_id,
             info.therapy_duration,
             info.last_passed_seconds);

    s_current_therapy_duration_s          = info.therapy_duration;
    s_therapy_passed_ms_before_last_pause = (uint32_t)info.therapy_passed_seconds * 1000u;
    s_current_therapy_state               = PAUSED;

    // last_passed_seconds is used as session clock baseline (in microseconds)
    reset_session_clock(info.last_passed_seconds * 1000000u);
    return true;
}

/**
 * Pauses the therapy due to an alert (temperature/humidity/proximity etc.).
 * If the timer is not running, logs an error and returns.
 */
void pause_therapy_because_of_alert(void)
{
    if (!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Cannot pause because of alert: therapy timer is not running.");
        return;
    }

    s_current_therapy_state = PAUSED;
    accumulate_passed_duration("Alert");

    stop_therapy_timer();
}

/**
 * Pauses therapy explicitly (e.g. by user action).
 * Aggregates passed time, stops timer, and starts inactivity timer.
 */
void pause_therapy(void)
{
    if (!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Cannot pause therapy: timer is not running.");
        return;
    }

    s_current_therapy_state = PAUSED;

    accumulate_passed_duration("Manual");
    ESP_LOGI(TAG,
             "Stored passed duration before last pause: %" PRIu32 " ms",
             s_therapy_passed_ms_before_last_pause);

    stop_therapy_timer();
    start_inactivity_timer();
}

/**
 * Terminates therapy completely and clears current therapy state.
 */
void terminate_therapy(void)
{
    if (is_therapy_timer_running()) {
        stop_therapy_timer();
    } else {
        ESP_LOGW(TAG, "terminate_therapy called but therapy timer is not running.");
    }
    clear_current_therapy();
}

/**
 * Sets up a new therapy session with the given total duration (seconds).
 * Clamps duration to MAX_THERAPY_DURATION if needed and resets passed time.
 */
static void set_new_therapy(uint16_t total_duration_s)
{
    ESP_LOGI(TAG, "Configuring new therapy.");

    if (total_duration_s == 0) {
        ESP_LOGW(TAG, "Attempting to set a zero-duration therapy.");
    }

    if (total_duration_s > MAX_THERAPY_DURATION) {
        ESP_LOGW(TAG,
                 "Requested therapy duration (%u s) exceeds max (%u s). Clamping.",
                 total_duration_s,
                 MAX_THERAPY_DURATION);
        total_duration_s = MAX_THERAPY_DURATION;
    }

    ESP_LOGI(TAG, "Current therapy duration is set to %u s.", total_duration_s);
    s_current_therapy_duration_s          = total_duration_s;
    s_therapy_passed_ms_before_last_pause = 0;
}

/**
 * Ensures that s_therapy_passed_ms_before_last_pause does not exceed plan duration.
 */
static bool is_therapy_passed_ms_before_last_pause_in_limit(void)
{
    uint32_t plan_ms = get_plan_duration_ms();

    if (s_therapy_passed_ms_before_last_pause >= plan_ms) {
        ESP_LOGE(TAG,
                 "Stored passed duration exceeds plan: %lu ms >= %lu ms",
                 (unsigned long)s_therapy_passed_ms_before_last_pause,
                 (unsigned long)plan_ms);
        s_therapy_passed_ms_before_last_pause = plan_ms;
        return false;
    }

    return true;
}

/**
 * Attempts to continue a paused therapy.
 * - is_by_app = true  → continued via app (different TimerState type)
 * - is_by_app = false → continued via button
 */
static void try_continue_therapy(bool is_by_app)
{
    if (s_therapy_passed_ms_before_last_pause == 0) {
        ESP_LOGE(TAG, "Cannot continue therapy: no stored passed time information.");
        return;
    }

    if (!is_therapy_passed_ms_before_last_pause_in_limit()) {
        return;
    }

    uint32_t plan_ms  = get_plan_duration_ms();
    uint32_t remain_ms = plan_ms - s_therapy_passed_ms_before_last_pause;
    uint16_t remain_s  = (uint16_t)((remain_ms + 999u) / 1000u);

    if (!is_by_app) {
        start_therapy_timer(remain_s, TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON);
    } else {
        start_therapy_timer(remain_s, TIMER_STATE_CONTINUE_THERAPY_BY_APP);
    }

    s_current_therapy_state = ACTIVE;
}

/**
 * Starts a new therapy from button trigger, or continues existing one:
 * - If no therapy configured yet, uses default duration and default brightness.
 * - Requires helmet_state == true.
 */
static void start_therapy_by_button(void)
{
    if (s_current_therapy_duration_s == 0) {
        ESP_LOGW(TAG, "No current therapy configured; using default configuration.");

        const uint8_t *brightness_list = get_default_brightness();

        for (int i = 0; i < TOTAL_REGION_COUNT; i++) {
            set_brightness_of_region((uint8_t)(i + 1), brightness_list[i]);
        }

        uint16_t default_therapy_duration = get_default_therapy_duration();
        set_new_therapy(default_therapy_duration);
        reset_session_clock(0);
    }

    ESP_LOGI(TAG, "Starting therapy timer (by button).");
    start_therapy_timer(s_current_therapy_duration_s, TIMER_STATE_NEW_THERAPY_BY_BUTTON);
    s_current_therapy_state = ACTIVE;
}

/**
 * Button-initiated flow to either start a new therapy or continue an existing one.
 * Also stops inactivity timer (if running) and checks helmet state.
 */
void start_or_continue_therapy_by_button(void)
{
    if (!is_inactivity_timer_running()) {
        ESP_LOGW(TAG, "Inactivity timer was not running when starting/continuing therapy.");
    } else if (!stop_inactivity_timer()) {
        ESP_LOGW(TAG, "Failed to stop inactivity timer when starting/continuing therapy.");
    }

    if (!get_helmet_state()) {
        ESP_LOGE(TAG, "Cannot start/continue therapy: helmet is not on.");
        return;
    }

    if (s_therapy_passed_ms_before_last_pause == 0) {
        start_therapy_by_button();
    } else {
        try_continue_therapy(false);
    }
}

/**
 * App-initiated continuation of a paused therapy.
 * Stops inactivity timer and checks helmet state before continuing.
 */
void continue_therapy_by_app(void)
{
    if (!is_inactivity_timer_running()) {
        ESP_LOGW(TAG, "Inactivity timer was not running when continuing therapy from app.");
    } else if (!stop_inactivity_timer()) {
        ESP_LOGW(TAG, "Failed to stop inactivity timer when continuing therapy from app.");
    }

    if (!get_helmet_state()) {
        ESP_LOGE(TAG, "Cannot continue therapy from app: helmet is not on.");
        return;
    }

    try_continue_therapy(true);
}

CurrentTherapyState get_current_therapy_state(void)
{
    return s_current_therapy_state;
}

uint16_t get_current_therapy_duration(void)
{
    return s_current_therapy_duration_s;
}

/**
 * Returns the current therapy ID from the persistent counter.
 * If there is no current therapy, returns 0.
 */
uint16_t get_current_therapy_id(void)
{
    if (s_current_therapy_state == NONE) {
        ESP_LOGI(TAG, "No active or paused therapy is present when requesting current therapy ID.");
        return 0;
    }

    uint16_t therapy_count = read_therapy_count();
    if (therapy_count == 0) {
        ESP_LOGW(TAG, "Therapy counter returned zero while a therapy state is active.");
    }

    return therapy_count;
}

/**
 * Returns the next therapy ID to be used for a new therapy.
 * If the counter reached MAX_THERAPY_COUNT, returns the current value.
 */
uint16_t get_new_therapy_id_for_new_therapy(void)
{
    uint16_t therapy_count = read_therapy_count();
    if (therapy_count >= MAX_THERAPY_COUNT) {
        ESP_LOGW(TAG, "Therapy counter reached the maximum value: %u", therapy_count);
        return therapy_count;
    }
    return (uint16_t)(therapy_count + 1u);
}

/**
 * Starts a brand new therapy initiated by the app (ActivationMessage).
 */
void start_new_therapy(uint16_t duration)
{
    if (duration == 0) {
        ESP_LOGE(TAG, "Cannot start a therapy with zero duration.");
        return;
    }

    set_new_therapy(duration);
    reset_session_clock(0);
    start_therapy_timer(duration, TIMER_STATE_NEW_THERAPY_BY_APP);
    s_current_therapy_state = ACTIVE;
}

/**
 * Returns the total passed therapy duration (seconds), including:
 * - stored paused duration
 * - plus current running timer if therapy is ACTIVE
 */
uint16_t get_current_therapy_passed_duration(void)
{
    uint32_t total_ms = s_therapy_passed_ms_before_last_pause;

    if (s_current_therapy_state == ACTIVE) {
        total_ms += get_therapy_passed_ms_direct();
    }

    uint16_t sec = (uint16_t)(total_ms / 1000u);
    ESP_LOGI(TAG,
             "get_current_therapy_passed_duration: stored=%lu ms, total=%lu ms (~%u s)",
             (unsigned long)s_therapy_passed_ms_before_last_pause,
             (unsigned long)total_ms,
             (unsigned)sec);

    return sec;
}

/**
 * Returns only the stored "passed duration before last pause" in seconds.
 */
uint16_t get_passed_duration_before_last_pause(void)
{
    return (uint16_t)(s_therapy_passed_ms_before_last_pause / 1000u);
}

/**
 * Initializes current therapy info manager and clears previous state.
 */
void init_current_therapy_info_manager(void)
{
    clear_current_therapy();
}
