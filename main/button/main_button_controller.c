#include "main_button_controller.h"

#include "../state/mode_selector.h"

#include "../transaction/notification_info_message_creator.h"
#include "../transaction/default_configuration_handler.h"

#include "../device_configuration.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const TickType_t k_short_press_window = pdMS_TO_TICKS(1000);

static uint16_t s_short_press_count = 0;
static TickType_t s_last_short_press_tick = 0;
static void (*button_press_callback)(ButtonPressType) = NULL;

static const char *TAG = "MainButtonController";


/**
 * @brief Handle a completed short-press sequence.
 *
 * @param press_count Number of short presses detected within k_short_press_window.
 *
 * - 1 press  : start or pause therapy.
 * - >1 press : change default parameters according to press count.
 */
static void on_short_press_sequence(uint16_t press_count)
{
    if (press_count == 1) {
        if (button_press_callback) button_press_callback(SHORT);
        //start_or_pause_therapy();
        return;
    }

    change_default_parameters(press_count);
}

/**
 * @brief Track a single short press and group presses into sequences.
 *
 * Eğer ardışık kısa basışlar k_short_press_window içinde gelirse sayılır,
 * aksi durumda önceki sekans finalize edilir ve yeni sekans başlatılır.
 */
static inline void track_short_press(void)
{
    TickType_t now = xTaskGetTickCount();

    if (s_short_press_count == 0) {
        s_short_press_count = 1;
    } else {
        if ((now - s_last_short_press_tick) <= k_short_press_window) {
            s_short_press_count++;
        } else {
            on_short_press_sequence(s_short_press_count);
            ESP_LOGI(TAG,
                     "Short press sequence finished (timeout). count=%lu",
                     (unsigned long)s_short_press_count);
            s_short_press_count = 1;
        }
    }

    s_last_short_press_tick = now;
}

/**
 * @brief Finalize short-press sequence if no new press arrived within window.
 *
 * Bu fonksiyon periyodik olarak çağrılır ve yeterince süre yeni basış gelmediyse
 * mevcut sekansı tamamlar.
 */
static inline void maybe_finalize_short_press_sequence(void)
{
    if (s_short_press_count > 0) {
        TickType_t now = xTaskGetTickCount();
        if ((now - s_last_short_press_tick) > k_short_press_window) {
            on_short_press_sequence(s_short_press_count);
            ESP_LOGI(TAG,
                     "Short press sequence finished (idle). count=%lu",
                     (unsigned long)s_short_press_count);
            s_short_press_count = 0;
        }
    }
}

/**
 * @brief Handle a short press based on indicator LED status.
 *
 * - Eğer indicator LED aktifse: kısa basışlar sekans olarak takip edilir.
 * - Değilse                 : tek basış olarak start_or_pause_therapy çağrılır.
 */
static void do_short_press(void)
{
    if (get_indicator_led_status()) {
        track_short_press();
        return;
    }

    if (button_press_callback) {
        button_press_callback(SHORT);
    }
}

/**
 * @brief Main button task: detect short / long / very long presses and act accordingly.
 *
 * Davranış:
 * - Kısa basış      : do_short_press() ile terapi kontrolü ve/veya default param değişimi
 * - Uzun basış      : mode indicator GPIO pin durumunu değiştirir
 * - Çok uzun basış  : NOTIF_SHUT_DOWN_BY_BUTTON log kaydı ile birlikte deep sleep'e geçer
 *
 * @param pvParameters FreeRTOS task parametresi (kullanılmıyor).
 */
void wait_for_button_to_sleep(void *pvParameters)
{
    const TickType_t poll_delay_ticks      = pdMS_TO_TICKS(100);
    const TickType_t long_press_ticks      = pdMS_TO_TICKS(PRESS_DURATION_TO_CONFIGURATION_MS);
    const TickType_t very_long_press_ticks = pdMS_TO_TICKS(PRESS_DURATION_TO_SLEEP_MS);

    TickType_t press_start_ticks = 0;
    bool button_was_pressed      = false;

    (void)pvParameters;

    while (1) {
        int button_level = gpio_get_level(BUTTON_GPIO);

        if (button_level == 0) {
            if (!button_was_pressed) {
                press_start_ticks = xTaskGetTickCount();
                button_was_pressed = true;
                ESP_LOGI(TAG, "Button pressed.");
            }
        } else if (button_was_pressed) {
            TickType_t press_duration_ticks = xTaskGetTickCount() - press_start_ticks;
            button_was_pressed = false;

#ifdef pdTICKS_TO_MS
            uint32_t duration_ms = (uint32_t)pdTICKS_TO_MS(press_duration_ticks);
#else
            uint32_t duration_ms = (uint32_t)press_duration_ticks * (uint32_t)portTICK_PERIOD_MS;
#endif

            ESP_LOGI(TAG, "Button released. Press duration=%lu ms", (unsigned long)duration_ms);

            if (press_duration_ticks >= very_long_press_ticks) {
                ESP_LOGI(TAG, "Very long press detected: entering deep sleep.");
                if (button_press_callback) button_press_callback(LONG);
            } else if (press_duration_ticks >= long_press_ticks) {
                ESP_LOGI(TAG, "Long press detected: toggling mode indicator.");
                change_mode_indicator_gpio_pin_status();
            } else {
                ESP_LOGI(TAG, "Short press detected.");
                do_short_press();
            }
        }

        maybe_finalize_short_press_sequence();

        vTaskDelay(poll_delay_ticks);
    }
}

void register_button_press_callback(void (*callback)(ButtonPressType)) {
    button_press_callback = callback;
}