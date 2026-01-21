#include "wifi_config_handler.h"

#include "../ble/include/ble_controller.h"

#include "esp_log.h"

#include "cJSON.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_sntp.h"


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "WifiConfigHandler";
static EventGroupHandle_t s_wifi_event_group;

static bool event_loop_created = false;
static bool netif_inited = false;
static bool handlers_registered = false;
static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_wifi_any_id = NULL;
static esp_event_handler_instance_t s_got_ip = NULL;

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
        ESP_LOGW(TAG, "Disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* e = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta_blocking(const char* ssid, const char* pass, uint32_t timeout_ms) {
    if (!netif_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        netif_inited = true;
    }
    if (!event_loop_created) {
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        event_loop_created = true;
    }

    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }

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

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }
    if (!handlers_registered) {
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &s_wifi_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &s_got_ip));
        handlers_registered = true;
    }

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
        ESP_LOGI(TAG, "Connected to SSID:%s", ssid);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Connect timeout/failed for SSID:%s", ssid);
        return ESP_FAIL;
    }
}
static esp_err_t do_ota(const char* url) {
    esp_http_client_config_t http_cfg = {
        .url = url,
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        // 🔧 kritik: bufferları büyüt
        .buffer_size = 4096,       // (en az 2048; 4096 güvenli)
        .buffer_size_tx = 4096,    // (redirect + header’lar için)
        // .user_agent = "esp32c6-ota",  // istersen
        // .event_handler = http_evt,   // istersen debug için
        // .header = "Accept: application/octet-stream\r\n", // opsiyonel
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
        // Büyük dosyalarda yardımcı olur:
        .partial_http_download = true,
        .max_http_request_size = 16384,  // 8K–32K arası da olur
    };

    esp_wifi_set_ps(WIFI_PS_NONE);

    for (int attempt = 1; attempt <= 3; ++attempt) {
        esp_err_t ret = esp_https_ota(&ota_cfg);
        if (ret == ESP_OK) { esp_restart(); }
        ESP_LOGW(TAG, "OTA attempt %d failed: %s", attempt, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return ESP_FAIL;
}

static void ota_task(void* arg) {
    char *ssid = ((char**)arg)[0];
    char *pass = ((char**)arg)[1];
    free(arg);

    if (wifi_init_sta_blocking(ssid, pass, 20000) == ESP_OK) {
        sntp_sync();
        ESP_LOGI(TAG, "Starting OTA from: %s", OTA_URL);
        if (do_ota(OTA_URL) != ESP_OK) {
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        }
    }
    free(ssid);
    free(pass);
    vTaskDelete(NULL);
}

static bool parse_wifi_json(const uint8_t* buf, size_t len, char* out_ssid, size_t ssid_sz,
                            char* out_pass, size_t pass_sz) {
    ESP_LOGI(TAG, "Parsing json.");
    if (!buf || !len) return false;
    char *tmp = malloc(len + 1);
    if (!tmp) return false;
    memcpy(tmp, buf, len); tmp[len] = '\0';

    cJSON *root = cJSON_Parse(tmp);
    free(tmp);
    if (!root) return false;

    cJSON *ju = cJSON_GetObjectItemCaseSensitive(root, "u");
    cJSON *jp = cJSON_GetObjectItemCaseSensitive(root, "p");
    bool ok = cJSON_IsString(ju) && ju->valuestring && cJSON_IsString(jp) && jp->valuestring;

    if (ok) {
        strlcpy(out_ssid, ju->valuestring, ssid_sz);
        strlcpy(out_pass, jp->valuestring, pass_sz);
    }
    cJSON_Delete(root);
    return ok;
}

static void on_wifi_config(const uint8_t *buf, size_t len) {
    char ssid[33] = {0};     // 32+1
    char pass[65] = {0};     // 64+1

    if (parse_wifi_json(buf, len, ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGI(TAG, "Wi-Fi JSON OK (u='%s', p=****)", ssid);

        char **holder = malloc(sizeof(char*) * 2);
        if (!holder) {
            ESP_LOGE(TAG, "Failed to allocate OTA task args");
            return;
        }
        holder[0] = strdup(ssid);
        holder[1] = strdup(pass);
        if (!holder[0] || !holder[1]) {
            ESP_LOGE(TAG, "Failed to allocate OTA credentials");
            free(holder[0]);
            free(holder[1]);
            free(holder);
            return;
        }
        if (xTaskCreate(ota_task, "ota_task", 8192, holder, 5, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Failed to create ota_task");
            free(holder[0]);
            free(holder[1]);
            free(holder);
        }
    }
}

void init_wifi_config() {
    register_on_write_wifi_config_callback(on_wifi_config);
}
