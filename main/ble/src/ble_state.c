#include "../include/ble_internal.h"

void (*on_connect_callback)(void) = NULL;
void (*on_disconnect_callback)(void) = NULL;
void (*on_write_activation_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_updating_records_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_updating_therapy_state_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_records_feedback_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_updating_passkey_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_updating_configuration_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_write_wifi_config_callback)(const uint8_t *buf, size_t len) = NULL;
void (*on_dynamic_period_change_callback)(uint16_t) = NULL;

esp_bd_addr_t g_peer_bda = {0};
bool          g_connected = false;

int8_t        g_rssi_ema = -60;
bool          g_rssi_task_running = false;
link_prof_t   g_prof = PROF_GOOD;
uint16_t      g_cur_mtu = 23;

SemaphoreHandle_t          s_conf_sem = NULL;
volatile esp_gatt_status_t s_last_conf_status = ESP_GATT_OK;
volatile uint16_t          s_last_conf_handle  = 0;

volatile bool g_ind_inflight = false;

uint8_t  s_adv_handle = 0;
bool     s_ext_adv_started = false;
uint8_t  s_adv_raw[31];
uint8_t  s_adv_len = 0;

// Profil tablosu
struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

uint16_t ms_to_conn_int(uint16_t ms) { return (uint16_t)((ms * 4 + 2) / 5); }
