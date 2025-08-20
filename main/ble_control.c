#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>


#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"
#include "sdkconfig.h"
#include "ble_control.h"

#ifndef UNIT_TESTING
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_bt.h"
#else
#include "fake_freertos.h"
#include "fake_task.h"
#include "fake_esp_log.h"
#include "fake_esp_err.h"
#include "fake_esp_mac.h"
#include "fake_semphr.h"
#include "fake_esp_system.h"
#include "fake_esp_bt.h"
#endif

#define PROFILE_NUM 1
#define MAX_JSON_STRING_SIZE 128
#define PROFILE_A_APP_ID 0
#define GATTS_CHAR_UUID_RECORDS                 0x2A56
#define GATTS_CHAR_UUID_TIMER_STATE             0x2A57
#define GATTS_CHAR_UUID_MEASUREMENT             0x2A58
#define GATTS_CHAR_UUID_NOTIFICATION            0x2A59
#define GATTS_CHAR_UUID_DEVICE                  0x2A5A
#define GATTS_CHAR_UUID_ACTIVATION              0x2A5B
#define GATTS_CHAR_UUID_UPDATING_RECORDS        0x2A5C
#define GATTS_CHAR_UUID_FEEDBACK                0x2A5D
#define GATTS_CHAR_UUID_UPDATING_THERAPY_STATE  0x2A5E
#define GATTS_CHAR_UUID_RECORDS_FEEDBACK        0x2A5F
#define ESP_GATT_UUID_CHAR_CLIENT_CONFIG        0x2902
#define GATTS_SERVICE_UUID16                    0x5555

static const char *TAG = "BLEControl";
static void (*on_connect_callback)() = NULL;
static void (*on_disconnect_callback)() = NULL;
static void (*on_write_activation_callback)(const char*) = NULL;
static void (*on_write_updating_records_callback)(const char*) = NULL;
static void (*on_write_feedback_callback)(const char*) = NULL;
static void (*on_write_updating_therapy_state_callback)(const char*) = NULL;
static void (*on_write_records_feedback_callback)(const char*) = NULL;
static void (*on_dynamic_period_change_callback) (uint16_t) = NULL;

static esp_bd_addr_t g_peer_bda = {0};
static bool g_connected = false;

static int8_t  g_rssi_ema = -60;
static bool g_rssi_task_running = false;
typedef enum { PROF_REALLY_GOOD, PROF_GOOD, PROF_FAIR, PROF_POOR, PROF_VERY_POOR, PROF_WORST} link_prof_t;
static link_prof_t g_prof = PROF_GOOD;


static SemaphoreHandle_t s_conf_sem = NULL;
static volatile esp_gatt_status_t s_last_conf_status = ESP_GATT_OK;
static volatile uint16_t s_last_conf_handle = 0;

static uint16_t notif_ind_cccd_handle = 0;
static bool notif_ind_enabled = false;


static inline uint16_t ms_to_conn_int(uint16_t ms) {
    return (uint16_t)((ms / 1.25f) + 0.5f);
}

extern void set_send_period_ms(uint16_t ms);
extern void set_bundle_size(uint8_t n);
extern void use_indication(bool on);
extern void use_indication_for_critical(bool on);

static inline esp_err_t set_phy_masks_(const esp_bd_addr_t addr,
                                       esp_ble_gap_phy_mask_t tx_mask,
                                       esp_ble_gap_phy_mask_t rx_mask,
                                       esp_ble_gap_prefer_phy_options_t opt)
{
    if (!g_connected) return ESP_ERR_INVALID_STATE;

    // all_phys_mask = 0 -> tx/rx mask'lerini kullan (no "all phys no pref")
    return esp_ble_gap_set_preferred_phy((uint8_t *)addr,
                                         /*all_phys_mask*/ 0,
                                         tx_mask, rx_mask, opt);
}

/* --- Hız (yakın mesafe) --- */
esp_err_t set_phy_2m(void)
{
    return set_phy_masks_(g_peer_bda,
                          ESP_BLE_GAP_PHY_2M_PREF_MASK,   /* TX */
                          ESP_BLE_GAP_PHY_2M_PREF_MASK,   /* RX */
                          ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
}

/* --- Orta (varsayılan hız/menzil) --- */
esp_err_t set_phy_1m(void)
{
    return set_phy_masks_(g_peer_bda,
                          ESP_BLE_GAP_PHY_1M_PREF_MASK,
                          ESP_BLE_GAP_PHY_1M_PREF_MASK,
                          ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
}

/* --- Uzun menzil (coded, S=2) --- */
esp_err_t set_phy_coded_s2(void)
{
    return set_phy_masks_(g_peer_bda,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
}

/* --- Uzun menzil (coded, S=8 – en dayanıklı) --- */
esp_err_t set_phy_coded_s8(void)
{
    return set_phy_masks_(g_peer_bda,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING);
}

/* --- Uzun menzil (coded, S2/S8 fark etmez) --- */
esp_err_t set_phy_coded_any(void)
{
    return set_phy_masks_(g_peer_bda,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                          ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
}

static link_prof_t choose_profile(int8_t rssi) {
    if (rssi >= -60) return PROF_REALLY_GOOD;
    if (rssi < -60 && rssi >= - 68) return PROF_REALLY_GOOD;
    if (rssi < -68 && rssi >= - 76) return PROF_FAIR;
    if (rssi < -76 && rssi >= - 84) return PROF_POOR;
    if (rssi < -84 && rssi >= - 92) return PROF_VERY_POOR;
    return PROF_WORST; 
}

static bool has_peer(void) {
    static const uint8_t zero[6] = {0};
    return memcmp(g_peer_bda, zero, 6) != 0;
}

static void request_conn_interval_ms(uint16_t target_ms, uint16_t timeout) {
    if (!has_peer()) {
        ESP_LOGW(TAG, "No peer BDA yet; skip conn param update");
        return;
    }
    esp_ble_conn_update_params_t prm = {
        .min_int = ms_to_conn_int(target_ms),
        .max_int = ms_to_conn_int(target_ms),
        .latency = 0,
        .timeout = 1000
    };
    memcpy(prm.bda, g_peer_bda, sizeof(esp_bd_addr_t));
    esp_err_t err = esp_ble_gap_update_conn_params(&prm);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_update_conn_params failed: %s", esp_err_to_name(err));
    }
}

static void apply_profile(link_prof_t p) {
    uint16_t connection_interval = 24;
    switch (p) {
        case PROF_GOOD: {
            connection_interval = 24;
            ESP_LOGI(TAG, "PROF_GOOD");
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(4 * connection_interval);
            }
            //set_bundle_size(3);
            request_conn_interval_ms(connection_interval, 8 * connection_interval);
            set_phy_2m();
            //use_indication(false);
            break;
        }
        case PROF_FAIR: {
            connection_interval = 36;
            ESP_LOGI(TAG, "PROF_FAIR");
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(5 * connection_interval);
            }
            //set_bundle_size(6);
            request_conn_interval_ms(connection_interval, 10 * connection_interval);
            set_phy_2m();
            //use_indication_for_critical(true);
            break;
        }
        case PROF_POOR: {
            ESP_LOGI(TAG, "PROF_POOR");
            connection_interval = 80;
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(6 * connection_interval);
            }
            //set_bundle_size(12);
            request_conn_interval_ms(connection_interval, 24 * connection_interval);
            set_phy_coded_s2();
            //use_indication(true);
            break;
        }
        default: {
            break;
        }
    }
}

static void rssi_poll_task(void *arg) {
    g_rssi_task_running = true;
    while (g_rssi_task_running) {
        // peer_bda set edildiyse oku
        if (memcmp(g_peer_bda, "\0\0\0\0\0\0", 6) != 0) {
            esp_ble_gap_read_rssi(g_peer_bda);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // ~0.8s
    }
    vTaskDelete(NULL);
}

/*
static const uint8_t GATTS_SERVICE_UUID128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 //TODO: Unique UUID yarat.
};
*/

void set_ble_tx_power() {
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P20);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TX power for connection handle 0: %s", esp_err_to_name(err));
    }

    // Reklam (Advertising) modunda maksimum gücü ayarla
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P20);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TX power for advertising: %s", esp_err_to_name(err));
    }
    
    // Tarama (Scanning) modunda maksimum gücü ayarla
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P20);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set TX power for scanning: %s", esp_err_to_name(err));
    }

    // Varsayılan güç seviyelerini ayarla
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P20);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set default TX power: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "BLE TX power set to maximum level (20 dBm).");
}

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    uint16_t records_handle;
    uint16_t timer_state_handle;
    uint16_t measurement_handle;
    uint16_t notification_handle;
    uint16_t device_handle;
    uint16_t activation_handle;
    uint16_t updating_records_handle;
    uint16_t feedback_handle;
    uint16_t updating_therapy_state_handle;        
    uint16_t records_feedback_handle;
    esp_bt_uuid_t char_uuid;
};

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       //Not get the gatt_if, so initial is ESP_GATT_IF_NONE
    },
};

static uint8_t s_adv_handle = 0;
static bool    s_ext_adv_started = false;

static uint8_t s_adv_raw[31];
static uint8_t s_adv_len = 0;

static void build_adv_data(void) {
    const char *name = "BLE-DA";
    uint8_t n = (uint8_t)strlen(name);
    s_adv_len = 0;
    s_adv_raw[s_adv_len++] = (uint8_t)(n + 1); // length
    s_adv_raw[s_adv_len++] = 0x09;             // AD Type: Complete Local Name
    memcpy(&s_adv_raw[s_adv_len], name, n);
    s_adv_len += n;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
        if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            int8_t r = param->read_rssi_cmpl.rssi;
            g_rssi_ema = (int8_t)(0.7f * g_rssi_ema + 0.3f * r);

            // her ~2sn’de profil uygula (basit pencere)
            static uint8_t tick = 0;
            if ((++tick % 2) == 0) {
                link_prof_t p = choose_profile(g_rssi_ema);
                if (p != g_prof) {
                    g_prof = p;
                    apply_profile(p);
                }
            }
        } else {
            ESP_LOGW(TAG, "RSSI read failed, status=%d", param->read_rssi_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT: {
        if (param->ext_adv_set_params.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "ext_adv_set_params failed, status=%d", param->ext_adv_set_params.status);
            break;
        }
        build_adv_data();
        esp_err_t err = esp_ble_gap_config_ext_adv_data_raw(s_adv_handle, s_adv_len, s_adv_raw);
        if (err != ESP_OK) ESP_LOGE(TAG, "config_ext_adv_data_raw failed: %s", esp_err_to_name(err));
        break;
    }

    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT: {
        if (param->ext_adv_data_set.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "ext_adv_data_set failed, status=%d", param->ext_adv_data_set.status);
            break;
        }
        esp_ble_gap_ext_adv_t start = {
            .instance     = s_adv_handle,
            .duration   = 0,    // 0 = sonsuz
            .max_events = 0,
        };
        esp_err_t err = esp_ble_gap_ext_adv_start(1, &start);
        if (err != ESP_OK) ESP_LOGE(TAG, "ext_adv_start failed: %s", esp_err_to_name(err));
        break;
    }

    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
        if (param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS) {
            s_ext_adv_started = true;
            ESP_LOGI(TAG, "Extended advertising started");
        } else {
            ESP_LOGE(TAG, "ext_adv_start failed, status=%d", param->ext_adv_start.status);
        }
        break;

    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
        s_ext_adv_started = false;
        ESP_LOGI(TAG, "Extended advertising stopped (status=%d)", param->ext_adv_stop.status);
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGW(TAG, "Conn params updated: status=%d, conn_int=%d, lat=%d, to=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(TAG, "Pkt length updated: rx=%d tx=%d status=%d",
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len,
                 param->pkt_data_length_cmpl.status);
        break;

    case ESP_GAP_BLE_ADV_TERMINATED_EVT:
        ESP_LOGW(TAG, "Ext adv terminated.");
        break;

    case ESP_GAP_BLE_SET_PREFERRED_PHY_COMPLETE_EVT: {
        esp_bt_status_t s = param->set_perf_phy.status;
        ESP_LOGI(TAG, "SET_PREF_PHY complete: status=%d", s);
        break;
    }

    case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT: {
        ESP_LOGI(TAG, "READ_PHY: status=%d tx=%d rx=%d",
                param->read_phy.status,
                param->read_phy.tx_phy,
                param->read_phy.rx_phy);
        break;
    }

    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT: {
        esp_bt_status_t s = param->phy_update.status;
        uint8_t tx = param->phy_update.tx_phy;
        uint8_t rx = param->phy_update.rx_phy;
        ESP_LOGW(TAG, "PHY UPDATE: status=%d tx=%u rx=%u", s, tx, rx);
        if (s == ESP_BT_STATUS_SUCCESS) {
            if (tx == ESP_BLE_GAP_PHY_CODED || rx == ESP_BLE_GAP_PHY_CODED) {
                ESP_LOGI(TAG, "Connected PHY is CODED (S=2 veya S=8; seçimi LL yaptı).");
            }
        } else {
            ESP_LOGW(TAG, "PHY update rejected; peer desteklemiyor olabilir, 1M/2M'de kaldık.");
        }
        break;
    }

    default:
        ESP_LOGW(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    //If event is register event, store the gatts_if for the profile
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if; // Save GATT interface for the profile
            esp_ble_gap_set_device_name("BLE-DA");

            ESP_LOGI(TAG, "GATTS_REG_EVT: Profile registered, app_id=%04x", param->reg.app_id);
        } else {
            ESP_LOGE(TAG, "GATTS_REG_EVT: Registration failed, app_id=%04x, status=%d",
                     param->reg.app_id, param->reg.status);
            return;
        }
    }

    // Dispatch events to the appropriate profile callback
    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) {
                gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }
}

static inline void parse_cccd_write(uint16_t descr_handle, const uint8_t *val, size_t len) {
    if (descr_handle != notif_ind_cccd_handle || len < 2) return;
    uint16_t cfg = val[0] | (val[1] << 8); // little-endian
    notif_ind_enabled = (cfg & 0x0002) != 0; // 0x0002 = Indication Enable
    ESP_LOGI(TAG, "Notification-char IND CCCD = %s", notif_ind_enabled ? "ENABLED" : "DISABLED");
}

static void add_cccd_for_char(uint16_t *cccd_out_handle) {
    esp_bt_uuid_t cccd_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}
    };
    ESP_LOGI(TAG, "Adding CCCD (0x2902)...");

    esp_err_t err = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &cccd_uuid,
                      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
    if (err != ESP_OK) ESP_LOGE(TAG, "add CCCD failed: %s", esp_err_to_name(err));
}


static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATT profile registered, app_id: %d", param->reg.app_id);
        esp_err_t err = esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = 0xFFF0

        }, 30); // Handle sayısı
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Service creation failed, error code: 0x%X", err);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
        ESP_LOGI(TAG, "Characteristic added, handle: %d, UUID: 0x%04X",
        param->add_char.attr_handle, param->add_char.char_uuid.uuid.uuid16);

        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_RECORDS) {
            gl_profile_tab[PROFILE_A_APP_ID].records_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Records Info Characteristic Handle: %d", param->add_char.attr_handle);
        } 
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_TIMER_STATE) {
            gl_profile_tab[PROFILE_A_APP_ID].timer_state_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Starting Therapy Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_MEASUREMENT) {
            gl_profile_tab[PROFILE_A_APP_ID].measurement_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Measurement Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_NOTIFICATION) {
            gl_profile_tab[PROFILE_A_APP_ID].notification_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Notification Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_DEVICE) {
            gl_profile_tab[PROFILE_A_APP_ID].device_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Device Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_ACTIVATION) {
            gl_profile_tab[PROFILE_A_APP_ID].activation_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Activation Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_UPDATING_RECORDS) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_records_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Updating Records Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_FEEDBACK) {
            gl_profile_tab[PROFILE_A_APP_ID].feedback_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Feedback Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_UPDATING_THERAPY_STATE) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Updating Therapy State Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_RECORDS_FEEDBACK) {
            gl_profile_tab[PROFILE_A_APP_ID].records_feedback_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Records Feedback Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "Service created, status: %d, handle: %d", param->create.status, param->create.service_handle);
        if (param->create.status == ESP_GATT_OK) {
            gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(param->create.service_handle);
        } else {
            ESP_LOGE(TAG, "Service creation failed with status: %d", param->create.status);
        }        
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
    
        esp_err_t add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_RECORDS},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding records info char failed, error code =%x", add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_TIMER_STATE},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding starting info char failed, error code =%x", add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_MEASUREMENT},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding measurement info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_NOTIFICATION},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding notification info char failed, error code =%x",add_char_ret);
        }

        add_cccd_for_char(&notif_ind_cccd_handle);

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_DEVICE},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding device info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_ACTIVATION},
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding activation info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_RECORDS},
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding updating records info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_FEEDBACK},
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding feedback info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_THERAPY_STATE},
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding updating therapy state info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_RECORDS_FEEDBACK},
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding records feedback info char failed, error code =%x",add_char_ret);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        //ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, handle: %d", param->write.handle);
        //ESP_LOGI(TAG, "WRITE: handle=%u len=%u first2=0x%02X%02X",
        //        param->write.handle, param->write.len,
        //        param->write.len>0?param->write.value[0]:0,
        //        param->write.len>1?param->write.value[1]:0);
        if (param->write.handle == notif_ind_cccd_handle) {
            parse_cccd_write(param->write.handle, param->write.value, param->write.len);
            ESP_LOGW(TAG, "CCCD is parsed");
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break; // CCCD yazımı ayrı; aşağıdaki app-logic yazımlarını tetikleme
        }

        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].activation_handle) {
            if(param->write.value != NULL && param->write.len > 0 && param->write.len < MAX_JSON_STRING_SIZE) {
                char json_str[MAX_JSON_STRING_SIZE];
                memcpy(json_str, param->write.value, param->write.len);
                json_str[param->write.len] = '\0';
                if (on_write_activation_callback) {
                    on_write_activation_callback(json_str);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_records_handle) {
            if(param->write.value != NULL && param->write.len > 0 && param->write.len < MAX_JSON_STRING_SIZE) {
                char json_str[MAX_JSON_STRING_SIZE];
                memcpy(json_str, param->write.value, param->write.len);
                json_str[param->write.len] = '\0';
                if (on_write_updating_records_callback) {
                    on_write_updating_records_callback(json_str);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].feedback_handle) {
            if(param->write.value != NULL && param->write.len > 0 && param->write.len < MAX_JSON_STRING_SIZE) {
                char json_str[MAX_JSON_STRING_SIZE];
                memcpy(json_str, param->write.value, param->write.len);
                json_str[param->write.len] = '\0';
                if (on_write_feedback_callback) {
                    on_write_feedback_callback(json_str);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle) {
            if(param->write.value != NULL && param->write.len > 0 && param->write.len < MAX_JSON_STRING_SIZE) {
                char json_str[MAX_JSON_STRING_SIZE];
                memcpy(json_str, param->write.value, param->write.len);
                json_str[param->write.len] = '\0';
                ESP_LOGI(TAG, "Write therapy state as %s", json_str);

                if (on_write_updating_therapy_state_callback) {
                    on_write_updating_therapy_state_callback(json_str);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].records_feedback_handle) {
            if(param->write.value != NULL && param->write.len > 0 && param->write.len < MAX_JSON_STRING_SIZE) {
                char json_str[MAX_JSON_STRING_SIZE];
                memcpy(json_str, param->write.value, param->write.len);
                json_str[param->write.len] = '\0';
                
                if (on_write_records_feedback_callback) {
                    on_write_records_feedback_callback(json_str);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Device disconnected, restarting advertising...");
        g_rssi_task_running = false;
        g_connected = false;
        memset(g_peer_bda, 0, sizeof(g_peer_bda));
        if (on_disconnect_callback) on_disconnect_callback();

        if (!s_ext_adv_started) {
            esp_ble_gap_ext_adv_t start = { .instance = s_adv_handle, .duration = 0, .max_events = 0 };
            esp_err_t err = esp_ble_gap_ext_adv_start(1, &start);
            if (err != ESP_OK) ESP_LOGE(TAG, "ext_adv_start (re) failed: %s", esp_err_to_name(err));
        }

        if (s_conf_sem) { vSemaphoreDelete(s_conf_sem); s_conf_sem = NULL; }
        notif_ind_enabled = false;
        break;
    
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected");
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
        memcpy(g_peer_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        g_connected = true;

        // başlangıç profili
        g_prof = PROF_GOOD;
        vTaskDelay(pdMS_TO_TICKS(300));
        apply_profile(g_prof);

        // RSSI poll’u başlat
        xTaskCreate(rssi_poll_task, "rssi_poll", 2048, NULL, 5, NULL);

        if (on_connect_callback) on_connect_callback();
        if (!s_conf_sem) {
            s_conf_sem = xSemaphoreCreateBinary();
        }
        notif_ind_enabled = false; // CCCD bağlantıya özeldir; resetle

        break;
    case ESP_GATTS_CONF_EVT:
        const bool is_notif = (param->conf.handle == gl_profile_tab[PROFILE_A_APP_ID].notification_handle);

        s_last_conf_status = param->conf.status;
        s_last_conf_handle = param->conf.handle;

        if (is_notif && param->conf.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "Indication CONF OK: handle=%u", param->conf.handle);
        } else if (is_notif) {
            ESP_LOGW(TAG, "Indication CONF failed: status=%d handle=%u",
                    param->conf.status, param->conf.handle);
        }

        if (s_conf_sem) xSemaphoreGive(s_conf_sem);
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        esp_bt_uuid_t *u = &param->add_char_descr.descr_uuid;

        bool is_cccd = false;
        if (u->len == ESP_UUID_LEN_16) {
            is_cccd = (u->uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG);
        } else if (u->len == ESP_UUID_LEN_128) {
            // 00002902-0000-1000-8000-00805F9B34FB
            static const uint8_t CCCD_128[16] = {
                0x02, 0x29, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
                0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB, 0x00, 0x00
            };
            is_cccd = (memcmp(u->uuid.uuid128, CCCD_128, 16) == 0);
        }

        ESP_LOGI(TAG, "ADD_CHAR_DESCR: handle=%u len=%d",
                param->add_char_descr.attr_handle, u->len);

        if (param->add_char_descr.status == ESP_GATT_OK && is_cccd) {
            notif_ind_cccd_handle = param->add_char_descr.attr_handle;
            ESP_LOGI(TAG, "CCCD (0x2902) added: handle=%u", notif_ind_cccd_handle);
        }
        break;  
    default:
        break;
    }
}

static void drain_conf_sem(void) {
    if (!s_conf_sem) return;
    while (xSemaphoreTake(s_conf_sem, 0) == pdTRUE) {}
}

static volatile bool g_ind_inflight = false;

static esp_err_t send_notification_char_as_indication(const uint8_t *data, size_t len, uint32_t timeout_ms) {

    if (!s_conf_sem) {
        ESP_LOGE(TAG, "s_conf_sem is null");
        return ESP_FAIL;
    }
    
    if (!notif_ind_enabled) {
        ESP_LOGW(TAG, "Client has not enabled indication (CCCD=0x0002 not set)");
        return ESP_GATT_INVALID_CFG;
    }
    
    if (g_ind_inflight) {
        ESP_LOGE(TAG, "another message is in flight.");
        return ESP_ERR_INVALID_STATE;
    } 

    drain_conf_sem();
    g_ind_inflight = true;

    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
        gl_profile_tab[PROFILE_A_APP_ID].notification_handle,
        len, (uint8_t*)data,
        true
    );

    if (ret != ESP_OK) { 
        g_ind_inflight = false; 
        ESP_LOGE(TAG, "ret is not ok: %s", esp_err_to_name(ret));
        return ret;
    }

    if (xSemaphoreTake(s_conf_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        g_ind_inflight = false;
        ESP_LOGE(TAG, "Indication CONF timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    g_ind_inflight = false;

    return (s_last_conf_status == ESP_GATT_OK) ? ESP_OK : ESP_FAIL;
}


static esp_err_t ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length)
{
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,  // GATT interface
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,   // Connection ID
        char_handle,                                // Characteristic handle
        data_length,                                // Data length
        data,                                       // Pointer to the data
        false                                        // Need confirmation?
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Successfully sent.");
    }
    return ret;
}

esp_err_t init_bluetooth() {
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    set_ble_tx_power();

    ESP_LOGI(TAG, "Bluetooth initialized successfully");
    return ESP_OK;
}


static uint16_t get_char_handle_by_message_type(MessageType type) {
    switch (type) {
        case RECORDS_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].records_handle;
        case TIMER_STATE_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].timer_state_handle;
        case MEASUREMENT_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].measurement_handle;
        case NOTIFICATION_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].notification_handle;
        case DEVICE_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].device_handle;
        default: return 0;
    }
}

esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length)
{
    if (message_type == NOTIFICATION_INFO_MESSAGE) {
        return send_notification_char_as_indication(data, data_length, /*timeout_ms*/ 10000);
    }

    return ble_send_message(get_char_handle_by_message_type(message_type), data, data_length);
}

void register_on_connect_callback(void (*callback)()) {
    on_connect_callback = callback;
}

void register_on_disconnect_callback(void (*callback)()) {
    on_disconnect_callback = callback;
}

void register_on_write_activation_callback(void (*callback)(const char*)) {
    on_write_activation_callback = callback;
}
void register_on_write_updating_records_callback(void (*callback)(const char*))
{
    on_write_updating_records_callback = callback;
}
void register_on_write_feedback_callback(void (*callback)(const char*))
{
    on_write_feedback_callback = callback;
}
void register_on_write_updating_therapy_state_callback(void (*callback)(const char*))
{
    on_write_updating_therapy_state_callback = callback;
}
void register_on_write_records_feedback_callback(void (*callback)(const char*))
{
    on_write_records_feedback_callback = callback;
}

void register_dynamic_period_change_callback(void (*callback)(uint16_t)) {
    on_dynamic_period_change_callback = callback;
}

esp_err_t start_registering_and_advertising()
{
    esp_err_t ret;

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    ESP_ERROR_CHECK(ret);

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }


    esp_ble_gap_ext_adv_params_t p = {
        .type           = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND, // opsiyonel
        .interval_min   = 0x20,
        .interval_max   = 0x40,
        .channel_map    = ADV_CHNL_ALL,
        .own_addr_type  = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        .primary_phy    = ESP_BLE_GAP_PHY_1M,   // uyumluluk için 1M öneririm
        .secondary_phy  = ESP_BLE_GAP_PHY_1M,   // bağlantı sonrası PHY’ı coded’a alacağız
        .max_skip       = 0,
        .tx_power       = 0x7F,                 // host seçsin (opsiyonel)
    };

    ret = esp_ble_gap_ext_adv_set_params(s_adv_handle, &p);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ext_adv_set_params failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Advertise başlatma çağrısı GAP event zinciri ile yapılacak:
    //   SET_PARAMS_COMPLETE -> DATA_SET_COMPLETE -> START
    return ret;
}