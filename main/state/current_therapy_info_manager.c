#include "current_therapy_info_manager.h"

#include "timer_manager.h"

#include "../storage/therapy_counter.h"

#include "../transaction/default_configuration_handler.h"

#include "../i2c/laser/laser_driver_controller.h"

#include "../device_configuration.h"

#include <stdint.h>
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "CurrentTherapyInfoManager";

static CurrentTherapyState current_therapy_state = NONE;
static uint16_t current_therapy_duration_s = 0;
static uint32_t passed_ms_before_last_pause = 0;

static inline uint32_t get_plan_duration_ms(void) {
    return (uint32_t)current_therapy_duration_s * 1000u;
}

static uint32_t clamp_elapsed_to_plan(uint32_t plan_ms, uint64_t candidate_ms) {
    if (candidate_ms > plan_ms) {
        ESP_LOGW(TAG,
                 "Elapsed duration overflow (candidate=%" PRIu64 ", plan=%" PRIu32 ")—clamping.",
                 candidate_ms, plan_ms);
        return plan_ms;
    }
    return (uint32_t)candidate_ms;
}

static void accumulate_passed_duration(const char *reason) {
    uint32_t plan_ms   = get_plan_duration_ms();
    uint32_t direct_ms = get_therapy_passed_ms_direct();
    uint64_t combined  = (uint64_t)passed_ms_before_last_pause + (uint64_t)direct_ms;

    uint32_t new_total = clamp_elapsed_to_plan(plan_ms, combined);
    ESP_LOGI(TAG,
             "%s pause: stored=%" PRIu32 "ms, direct=%" PRIu32 "ms, plan=%" PRIu32 "ms => new=%" PRIu32 "ms",
             reason, passed_ms_before_last_pause, direct_ms, plan_ms, new_total);
    passed_ms_before_last_pause = new_total;
}


static void clear_current_therapy(void) {
    ESP_LOGI(TAG, "Clear current therapy.");
    current_therapy_duration_s = 0;
    current_therapy_state = NONE;
    passed_ms_before_last_pause = 0;
    clear_session_clock();
}

void pause_therapy_because_of_alert(void) {
    if (!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Therapy timer is not running when pausing because of alert.");
        return;
    }
    current_therapy_state = PAUSED;
    accumulate_passed_duration("Alert");

    stop_therapy_timer();
}

void pause_therapy(void) {
    if (!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Therapy timer is not running when pausing.");
        return;
    }
    current_therapy_state = PAUSED;

    accumulate_passed_duration("Manual");
    ESP_LOGI(TAG, "passed_ms_before_last_pause: %" PRIu32, passed_ms_before_last_pause);

    stop_therapy_timer();
    start_inactivity_timer();
}

void terminate_therapy() {
    if(!is_therapy_timer_running()) {
        ESP_LOGE(TAG, "Therapy timer is not running when terminating.");
        clear_current_therapy();
        return;
    }
    stop_therapy_timer();
    clear_current_therapy();
}

static void set_new_therapy(uint16_t total_duration_s) {
    ESP_LOGI(TAG, "Set new therapy.");
    if (total_duration_s == 0) {
        ESP_LOGW(TAG, "Attempting to set a zero-duration therapy.");
    }
    current_therapy_duration_s = total_duration_s;
    passed_ms_before_last_pause = 0;
}

static void start_therapy(bool is_by_app) {
    uint32_t plan_ms = get_plan_duration_ms();

    if(passed_ms_before_last_pause == 0) {
        if(!is_by_app) {
            if(current_therapy_duration_s == 0) {
                ESP_LOGW(TAG, "No current therapy; using default configuration");

                uint8_t* brightness_list = get_default_brightness();

                for(int i = 0; i < TOTAL_REGION_COUNT; i++) {
                    set_brightness_of_region((uint8_t)(i + 1), brightness_list[i]);
                }

                uint16_t default_therapy_duration = get_default_therapy_duration();
                set_new_therapy(default_therapy_duration);
                reset_session_clock();
                plan_ms = get_plan_duration_ms();
            }
            ESP_LOGE(TAG, "Set therapy timer");
            start_therapy_timer(current_therapy_duration_s, TIMER_STATE_NEW_THERAPY_BY_BUTTON);
            current_therapy_state = ACTIVE;
        }
        else{
            ESP_LOGE(TAG, "App does not start default therapy.");
        }
    }
    else if(passed_ms_before_last_pause >= plan_ms) {
        ESP_LOGE(TAG, "Passed exceeds plan: %lu >= %lu", (unsigned long)passed_ms_before_last_pause, (unsigned long)plan_ms);
        passed_ms_before_last_pause = plan_ms;
        return;
    }
    else {
        current_therapy_state = ACTIVE;
        uint32_t remain_ms = plan_ms - passed_ms_before_last_pause;
        uint16_t remain_s = (uint16_t)((remain_ms + 999u) / 1000u);
        if(!is_by_app) {
            start_therapy_timer(remain_s, TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON);
        }
        else{
            start_therapy_timer(remain_s, TIMER_STATE_CONTINUE_THERAPY_BY_APP);
        }
    }
}

void start_or_continue_therapy(bool is_by_app) {
    if(!is_inactivity_timer_running()) {
        ESP_LOGW(TAG, "Inactivity timer is not running when continuing therapy.");
    } else if (!stop_inactivity_timer()) {
        ESP_LOGW(TAG, "Failed to stop inactivity timer when continuing therapy.");
    }

    if(!get_helmet_state()) {
        ESP_LOGE(TAG, "Helmet is not on.");
        return;
    }
    current_therapy_state = ACTIVE;
    stop_inactivity_timer();
    start_therapy(is_by_app);
}

CurrentTherapyState get_current_therapy_state() {
    return current_therapy_state;
}

uint16_t get_current_therapy_duration() {
    return current_therapy_duration_s;
}

uint16_t get_current_therapy_id() {
    if(current_therapy_state == NONE) {
        ESP_LOGI(TAG, "There is not a current therapy");
        return 0;
    }
    uint16_t therapy_count = read_therapy_count();
    if (therapy_count == 0) {
        ESP_LOGW(TAG, "Therapy counter returned zero while a therapy is active.");
    }
    return therapy_count;
}

uint16_t get_new_therapy_id_for_new_therapy() {
    uint16_t therapy_count = read_therapy_count();
    if (therapy_count >= MAX_THERAPY_COUNT) {
        ESP_LOGW(TAG, "Therapy counter reached the maximum value: %u", therapy_count);
        return therapy_count;
    }
    return (uint16_t)(therapy_count + 1u);
}

void start_new_therapy(uint16_t duration) {
    if (duration == 0) {
        ESP_LOGE(TAG, "Cannot start a therapy with zero duration.");
        return;
    }
    set_new_therapy(duration);
    reset_session_clock();
    start_therapy_timer(duration, TIMER_STATE_NEW_THERAPY_BY_APP);
    current_therapy_state = ACTIVE;
}



uint16_t get_current_therapy_passed_duration(void) {
    uint32_t total_ms = passed_ms_before_last_pause;
    if (current_therapy_state == ACTIVE) {
        total_ms += get_therapy_passed_ms_direct();
    }
    uint16_t sec = (uint16_t)(total_ms / 1000u);
    ESP_LOGI(TAG, "passed_ms_before_last_pause=%lu, total_ms=%lu (~%u s)",
             (unsigned long)passed_ms_before_last_pause,
             (unsigned long)total_ms, (unsigned)sec);
    return sec;
}

uint16_t get_passed_duration_before_last_pause(void) {
    return (uint16_t)((passed_ms_before_last_pause) / 1000u);
}

void init_current_therapy_info_manager(void) {
    clear_current_therapy();
}