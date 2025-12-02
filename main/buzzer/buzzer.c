// buzzer.c
#include "buzzer.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUZZER_LEDC_TIMER      LEDC_TIMER_0
#define BUZZER_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL    LEDC_CHANNEL_0
#define BUZZER_LEDC_RES        LEDC_TIMER_10_BIT   // 10 bit: max duty = 1023
#define BUZZER_DEFAULT_FREQ_HZ 4000                // piezo için tipik frekans

static bool s_buzzer_inited = false;

esp_err_t buzzer_init(int gpio_num)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode       = BUZZER_LEDC_MODE,
        .timer_num        = BUZZER_LEDC_TIMER,
        .duty_resolution  = BUZZER_LEDC_RES,
        .freq_hz          = BUZZER_DEFAULT_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t ch_conf = {
        .gpio_num       = gpio_num,
        .speed_mode     = BUZZER_LEDC_MODE,
        .channel        = BUZZER_LEDC_CHANNEL,
        .timer_sel      = BUZZER_LEDC_TIMER,
        .duty           = 0,         // başlangıçta sessiz
        .hpoint         = 0,
        .intr_type      = LEDC_INTR_DISABLE
    };

    ret = ledc_channel_config(&ch_conf);
    if (ret == ESP_OK) {
        s_buzzer_inited = true;
    }
    return ret;
}

static uint32_t volume_to_duty(uint8_t volume_percent)
{
    if (volume_percent > 100) volume_percent = 100;
    uint32_t max_duty = (1 << BUZZER_LEDC_RES) - 1; // mesela 1023
    return (max_duty * volume_percent) / 100;
}

void buzzer_set_tone(uint32_t freq_hz, uint8_t volume_percent)
{
    if (!s_buzzer_inited) return;

    if (freq_hz == 0 || volume_percent == 0) {
        // tamamen sustur
        ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
        ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
        return;
    }

    // frekansı ayarla
    ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, freq_hz);

    // duty'i ayarla (ses seviyesi)
    uint32_t duty = volume_to_duty(volume_percent);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, duty);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

void buzzer_stop(void)
{
    buzzer_set_tone(0, 0);
}

void buzzer_beep(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume_percent)
{
    if (!s_buzzer_inited) return;

    buzzer_set_tone(freq_hz, volume_percent);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_stop();
}

// Kısa yükselen 2 notalı "başarılı" sesi
void buzzer_success_tone(void)
{
    // 1. nota: 3.5 kHz, kısa
    buzzer_beep(3500, 120, 70);
    vTaskDelay(pdMS_TO_TICKS(40));
    // 2. nota: 4.2 kHz, biraz daha uzun
    buzzer_beep(4200, 160, 70);
}

// İki kısa kalın bip: "hata"
void buzzer_error_tone(void)
{
    buzzer_beep(2000, 150, 80);
    vTaskDelay(pdMS_TO_TICKS(80));
    buzzer_beep(1800, 200, 80);
}

// Bildirim gibi 3 notalı seri bip
void buzzer_notification_tone(void)
{
    uint32_t freqs[] = { 3000, 3500, 4000 };
    for (int i = 0; i < 3; ++i) {
        buzzer_beep(freqs[i], 90, 60);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void buzzer_therapy_start_tone(void)
{
    // Yükselen 3 ton: terapi başlangıcı
    buzzer_beep(3000, 90, 60);   // 3.0 kHz, 90 ms
    vTaskDelay(pdMS_TO_TICKS(40));

    buzzer_beep(3500, 90, 60);   // 3.5 kHz, 90 ms
    vTaskDelay(pdMS_TO_TICKS(40));

    buzzer_beep(4000, 150, 60);  // 4.0 kHz, 150 ms
}