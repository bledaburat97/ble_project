
#ifndef CURRENT_THERAPY_STATE_MANAGER_H
#define CURRENT_THERAPY_STATE_MANAGER_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef enum {
    ACTIVE = 0,
    PAUSED,
    NONE,
} CurrentTherapyState;

void set_current_therapy_state(CurrentTherapyState state);
CurrentTherapyState get_current_therapy_state();

#endif