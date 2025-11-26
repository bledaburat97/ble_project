#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t buzzer_init(int gpio_num); //TODO: call in main
void buzzer_set_tone(uint32_t freq_hz, uint8_t volume_percent);
void buzzer_beep(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_percent);
void buzzer_stop(void);

void buzzer_success_tone(void);
void buzzer_error_tone(void);
void buzzer_notification_tone(void);
void buzzer_therapy_start_tone(void);