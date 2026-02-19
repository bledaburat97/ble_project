#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "device_configuration.h"

#include "state/deep_sleep_manager.h"
#include "storage/log_types.h"
#include "manager/message_saver.h"

#include "manager/device_initiator.h"

static const char *TAG = "Main";

/**
 * ESP-IDF NVS flash init (platform bootstrap)
 */
static void initialize_nvs_flash_module(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/**
 * Eğer default_sleep_active açıksa, uyanma sebebini kontrol eder:
 * - EXT1 ve beklenen GPIO ise devam
 * - Değilse deep sleep
 */
static bool activate_device_if_needed(bool default_sleep_active)
{
    if (!default_sleep_active) {
        return true;
    }

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Wakeup by EXT1.");
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & BUTTON_PIN_BITMASK) {
            ESP_LOGI(TAG, "Wakeup from expected GPIO.");
            return true;
        }
        ESP_LOGW(TAG, "Wakeup not from expected GPIO pin.");
        return false;
    }

    ESP_LOGI(TAG, "Not a valid wakeup source. Entering deep sleep.");
    save_log(NOTIF_DEVICE_NOT_AWAKED, NULL, 0, 0);
    enter_deep_sleep();
    return false;
}

void app_main(void)
{
    initialize_nvs_flash_module();

    const bool default_sleep_active = false;

    if (!activate_device_if_needed(default_sleep_active)) {
        return;
    }

    DeviceFeatureConfig cfg = {
        .ble_active = true,
        .state_and_timer_active = true,
        .lp_prox_sensor_active = true,
        .hp_prox_sensor_active = true,

        .therapy_counter_partition_active = true,
        .log_partition_active = true,
        .log_orchestrator_active = true,

        .laser_and_led_drivers_active = true,
        .apply_default_brightness_on_boot = true,

        .deep_sleep_button_control_active = true,
        .creating_logs_permitted = true,

        .continue_uncompleted_therapy = false,

        .mode_indicator_active = true,
        .wifi_config_active = true,
    };

    device_initiator_start(&cfg);

    ESP_LOGI(TAG, "System bootstrap done. Device initiator running.");
}


