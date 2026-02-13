#include "current_therapy_state_manager.h"

#include <stdint.h>
#include "esp_log.h"
#include <stdbool.h>

static CurrentTherapyState s_current_therapy_state = NONE;

void set_current_therapy_state(CurrentTherapyState state) {
    s_current_therapy_state = state;
}

CurrentTherapyState get_current_therapy_state() {
    return s_current_therapy_state;
}