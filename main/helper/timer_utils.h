#include <stdint.h>

#ifndef TIMER_UTILS_H
#define TIMER_UTILS_H

#include "../state/state_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "time.h"
#include "freertos/timers.h"

TimerHandle_t create_and_start_timer(DeviceState state, uint32_t duration_ms, TimerCallbackFunction_t callback);

bool stop_and_delete_timer(TimerHandle_t* timer);

#endif