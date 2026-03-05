#include "profile_manager.h"
#include "../storage/profile_partition_manager.h"
#include "esp_log.h"

static const char *TAG = "ProfileManager";

static uint32_t active_profile_id = 0;
static bool is_profile_changed = false;
static bool is_first_profile = false;


void set_profile_info(uint32_t profile_id, bool profile_changed, bool first_profile) {
    active_profile_id = profile_id;
    is_profile_changed = profile_changed;
    is_first_profile = first_profile;
}


void try_append_profile(uint16_t therapy_id) {
    if(is_profile_changed) {
        if(is_first_profile) {
            ESP_LOGW(TAG, "First profile is appended.");
            profile_append(active_profile_id, 1);
        }
        else{
            profile_append(active_profile_id, therapy_id);
        }
        is_profile_changed = false;
        ESP_LOGW(TAG, "New profile is appended.");
    }
}