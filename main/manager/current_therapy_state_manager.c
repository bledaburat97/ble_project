#include "current_therapy_state_manager.h"

#include <stdint.h>
#include "esp_log.h"
#include <stdbool.h>

// Cihazın o anki terapi durumu.
static CurrentTherapyState s_current_therapy_state = NONE;

// Terapi durumunu set eder.
void set_current_therapy_state(CurrentTherapyState state) {
    s_current_therapy_state = state;
}

// Terapi durumunu okur.
CurrentTherapyState get_current_therapy_state() {
    return s_current_therapy_state;
}
