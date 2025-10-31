#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "transaction/default_configuration_handler.h"
#include "transaction/matching_message_encoder.h"
#include "transaction/notification_info_message_creator.h"
#include "transaction/passkey_handler.h"
#include "transaction/records_info_message_creator.h"
#include "transaction/measurement_info_message_creator.h"
#include "transaction/timer_state_info_message_creator.h"
#include "transaction/message_queue_manager.h"
#include "transaction/incoming_message_handler.h"
#include "transaction/device_info_message_creator.h"

#include "state/deep_sleep_manager.h"
#include "state/general_manager.h"
#include "state/timer_manager.h"
#include "state/state_manager.h"
#include "state/mode_selector.h"

#include "i2c/i2c_control.h"
#include "i2c/laser/laser_driver_controller.h"
#include "i2c/proximity/proximity_int_controller.h"
#include "i2c/proximity/proximity_sensor_config.h"
#include "i2c/proximity/proximity_sensor_controller.h"

#include "i2c/temperature/temperature_alert_controller.h"
#include "i2c/temperature/temperature_sensor_controller.h"

#include "button/main_button_controller.h"

#include "storage/log_partition_manager.h"
#include "storage/log_types.h"
#include "storage/log_utils.h"
#include "storage/log_writer.h"
#include "storage/therapy_counter.h"

#include "nvs/storage_manager.h"

#include "ble/include/ble_controller.h"

#include "lp_core/lp_core_main.h"
#include "lp_core/lp_core_queue_manager.h"

#include "lp_core_firmware.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_sntp.h"

static const char *TAG = "Main";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
const char* OTA_URL = "https://github.com/bledaburat97/ble_project/releases/latest/download/app.bin";

static void sntp_sync(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    // basit bloklayıcı bekleme (prod'da event tabanlı yapabilirsin)
    for (int i = 0; i < 20; ++i) {
        time_t now = 0; struct tm tm_info = {0};
        time(&now); localtime_r(&now, &tm_info);
        if (tm_info.tm_year >= (2020 - 1900)) break; // yıl mantıklı olduysa
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Tekrar dene
        esp_wifi_connect();
        ESP_LOGW("WIFI", "Disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* e = (ip_event_got_ip_t*)event_data;
        ESP_LOGI("WIFI", "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta_blocking(const char* ssid, const char* pass, uint32_t timeout_ms) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    // WPA3/PMF uyumluluğu için güvenli varsayılanlar:
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("WIFI", "Connected to SSID:%s", ssid);
        return ESP_OK;
    } else {
        ESP_LOGE("WIFI", "Connect timeout/failed for SSID:%s", ssid);
        return ESP_FAIL;
    }
}

static esp_err_t do_ota(const char* url) {
    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = 30000, // OTA için biraz geniş
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
        // .user_agent = "esp32c6-ota" // istersen ekle
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        // .partial_http_download = true, // çok büyük imajlarda faydalı olabilir
    };

    // Wi-Fi güç tasarrufunu geçici kapat
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Basit retry
    for (int attempt = 1; attempt <= 3; ++attempt) {
        esp_err_t ret = esp_https_ota(&ota_cfg);
        if (ret == ESP_OK) {
            ESP_LOGI("OTA", "OTA OK (attempt %d), rebooting...", attempt);
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }
        ESP_LOGW("OTA", "OTA attempt %d failed: %s", attempt, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGE("OTA", "All attempts failed");
    return ESP_FAIL;
}

static void ota_task(void* arg) {
    const char* SSID = "Bleda's iphone";
    const char* PASS = "Bleda124";

    if (wifi_init_sta_blocking(SSID, PASS, 20000) == ESP_OK) {
        sntp_sync(); // <--- ZAMAN SENKRONU EKLENDİ
        ESP_LOGI("OTA", "Starting OTA from: %s", OTA_URL);
        do_ota(OTA_URL);
    }
    vTaskDelete(NULL);
}


void periodic_message_sender_task(void *pvParameters) {
    const TickType_t delay_ticks = pdMS_TO_TICKS(30 * 1000); 
    while (1) {
        ESP_LOGI(TAG, "Sending records info message for therapy ID 1...");
        send_records_info_message(1);

        vTaskDelay(delay_ticks);
    }
}

typedef struct {
    bool ble_active;
    bool state_and_timer_active;
    bool lp_prox_sensor_active;
    bool therapy_counter_partition_active;
    bool log_partition_active;
    bool laser_and_led_drivers_active;
    bool temperature_sensor_active;
    bool hp_prox_sensor_active;
    bool default_sleep_active;
    bool deep_sleep_button_control_active;
    bool creating_logs_permitted;
} feature_config_t;

static const feature_config_t feature_config = {
    .ble_active = true,
    .state_and_timer_active = true,
    .lp_prox_sensor_active = false,
    .therapy_counter_partition_active = true,
    .log_partition_active = true,
    .laser_and_led_drivers_active = true,
    .temperature_sensor_active = true,
    .hp_prox_sensor_active = false,
    .default_sleep_active = false,
    .deep_sleep_button_control_active = true,
    .creating_logs_permitted = true,
};

static void initialize_nvs_flash_module(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
 
static bool activate_device_if_needed(const feature_config_t *config) {
    if (!config->default_sleep_active) {
        return true;
    }
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Button is pressed.");
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pins & BUTTON_PIN_BITMASK) {
            ESP_LOGI(TAG, "Awakes by GPIO!");
            return true;
        }
        ESP_LOGW(TAG, "Wake-up did not originate from the expected GPIO pin.");
        return false;
    }
    ESP_LOGI(TAG, "Deep sleep.");
        enter_deep_sleep();
        return false;
}

static void initialize_storage_components(const feature_config_t *config) {
    init_nvs();

    if (config->therapy_counter_partition_active) {
        init_therapy_counter_partition();
        uint16_t therapy_count = read_therapy_count();
        ESP_LOGI(TAG, "Therapy count: %u", therapy_count);
    }
    if (config->log_partition_active) {
        init_log_partition();
    }
}

static void init_message_creators(){
    init_notification_info_message_creator();
    init_device_info_message_creator();
    init_measurement_info_message_creator();
    init_timer_state_info_message_creator();
    init_records_info_message_creator();
}

static void initialize_ble_components(const feature_config_t *config) {
    if (config->ble_active) {
        init_ble();
        init_message_queue_manager();
        init_incoming_message_handler();
        init_passkey_handler();
        init_default_configuration_handler();
        init_message_creators();
    }

}

static void initialize_state_management_components(const feature_config_t *config) {
    if (config->state_and_timer_active) {
        init_timer_manager();
        init_general_manager();
    }
}

static void initialize_lp_core_components(const feature_config_t *config) {
    if (!config->lp_prox_sensor_active) {
        return;
    }

    init_lp_i2c_master();
    initialize_lp_core();
    initialize_lp_core_queue();
    xTaskCreate(process_lp_queue_task, "ProcessLpQueueTask", 4096, NULL, 5, NULL);
}

static void initialize_temperature_components(const feature_config_t *config) {
    if (!config->temperature_sensor_active) {
        return;
    }
    initialize_temperature_sensor();
    initialize_alert_gpios();
    xTaskCreate(temperature_read_task, "Temperature Update Task", 2048, NULL, 1, NULL);
    //xTaskCreate(monitor_alert_task, "Monitor Alert Task", 2048, NULL, 1, NULL);
}

static void initialize_proximity_components(const feature_config_t *config) {
    if (!config->lp_prox_sensor_active && !config->hp_prox_sensor_active) {
        return;
    }
    initialize_proximity_int_gpio();
    initialize_proximity_sensors(config->hp_prox_sensor_active, config->lp_prox_sensor_active);
    xTaskCreate(monitor_proximity_int_task, "Monitor Proximity Int Task", 2048, NULL, 1, NULL);
    //xTaskCreate(proximity_read_task, "ProximityReadTask", 2048, NULL, 5, NULL);
}

static void initialize_laser_components(const feature_config_t *config) {
    if (!config->laser_and_led_drivers_active) {
        return;
    }
    initialize_laser_drivers();
    const uint8_t *brightness_list = get_default_brightness();

    for (int i = 0; i < TOTAL_REGION_COUNT; i++) {
        set_brightness_of_region(i + 1, brightness_list[i]);
    }

    //set_brightness_of_region(1, 20);
    //set_brightness_of_region(2, 20);
    //set_brightness_of_region(3, 20);
    //set_brightness_of_region(4, 20);
}

static void initialize_sensor_and_driver_components(const feature_config_t *config) {
    init_i2c_master();
    initialize_lp_core_components(config);
    initialize_laser_components(config);
    initialize_temperature_components(config);
    initialize_proximity_components(config);
}

static void initialize_button_components(const feature_config_t *config) {
    if (config->deep_sleep_button_control_active) {
        set_deep_sleep_button();
        xTaskCreate(wait_for_button_to_sleep, "button_task", 2048, NULL, 1, NULL);
    }
}

static void finalize_device_startup(const feature_config_t *config) {
    if (config->creating_logs_permitted) {
        add_and_send_notification_info(DEVICE_AWAKED);
    }

    ESP_LOGI(TAG, "Device awakes.");

    if (config->state_and_timer_active) {
        start_device();
    }

    ESP_LOGI(TAG, "System Ready.");
}



void app_main(void) {
    initialize_nvs_flash_module();

    if (!activate_device_if_needed(&feature_config)) {
        return;
    }

    initialize_storage_components(&feature_config);
    initialize_ble_components(&feature_config);
    initialize_state_management_components(&feature_config);

    //test_add_log_flow();
    //xTaskCreate(periodic_message_sender_task, "PeriodicMsgSender", 2048, NULL, 5, NULL);

    /*
        read_and_set_records(0);
        read_and_set_records(1);
        read_and_set_records(2);
        read_and_set_records(3);
        ESP_LOGI(TAG, "fragment count: %u", get_fragment_count());
    */

    initialize_sensor_and_driver_components(&feature_config);
    initialize_button_components(&feature_config);
    finalize_device_startup(&feature_config);
    initialize_mode_indicator_gpio();
    //xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
}