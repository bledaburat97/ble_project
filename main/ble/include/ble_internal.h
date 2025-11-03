#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_bt_defs.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "ble_controller.h"

// ---- Sabitler / UUID’ler (mevcut değerleri koruyoruz) ----
#define PROFILE_NUM 1
#define MAX_JSON_STRING_SIZE 128
#define PROFILE_A_APP_ID 0
#define GATTS_CHAR_UUID_WIFI_CONFIG             0x2A53
#define GATTS_CHAR_UUID_UPDATING_CONFIG         0x2A54
#define GATTS_CHAR_UUID_AUTH                    0x2A55
#define GATTS_CHAR_UUID_RECORDS                 0x2A56
#define GATTS_CHAR_UUID_TIMER_STATE             0x2A57
#define GATTS_CHAR_UUID_MEASUREMENT             0x2A58
#define GATTS_CHAR_UUID_NOTIFICATION            0x2A59
#define GATTS_CHAR_UUID_DEVICE                  0x2A5A
#define GATTS_CHAR_UUID_ACTIVATION              0x2A5B
#define GATTS_CHAR_UUID_UPDATING_RECORDS        0x2A5C
#define GATTS_CHAR_UUID_UPDATING_PASSKEY        0x2A5D
#define GATTS_CHAR_UUID_UPDATING_THERAPY_STATE  0x2A5E
#define GATTS_CHAR_UUID_RECORDS_FEEDBACK        0x2A5F
#define ESP_GATT_UUID_CHAR_CLIENT_CONFIG        0x2902
#define GATTS_SERVICE_UUID16                    0x5555

// ---- Link kalitesi enum’u ----
typedef enum { PROF_REALLY_GOOD, PROF_GOOD, PROF_FAIR, PROF_POOR, PROF_VERY_POOR, PROF_WORST } link_prof_t;

// ---- Profil tablosu ----
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    uint16_t auth_handle;
    uint16_t records_handle;
    uint16_t timer_state_handle;
    uint16_t measurement_handle;
    uint16_t notification_handle;
    uint16_t device_handle;
    uint16_t activation_handle;
    uint16_t updating_records_handle;
    uint16_t updating_therapy_state_handle;
    uint16_t updating_passkey_handle;
    uint16_t updating_configuration_handle;
    uint16_t records_feedback_handle;
    uint16_t wifi_config_handle;
    esp_bt_uuid_t char_uuid;
};

// ---- Global (tek tanım ble_state.c’de), burada extern ----
extern const char *BLE_TAG;
extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

extern void (*on_connect_callback)(void);
extern void (*on_disconnect_callback)(void);
extern void (*on_write_activation_callback)(const uint8_t *buf, size_t len);
extern void (*on_write_updating_records_callback)(const uint8_t *buf, size_t len);
//extern void (*on_write_feedback_callback)(const char*);
extern void (*on_write_updating_therapy_state_callback)(const uint8_t *buf, size_t len);
extern void (*on_write_records_feedback_callback)(const uint8_t *buf, size_t len);
extern void (*on_write_updating_passkey_callback)(const uint8_t *buf, size_t len);
extern void (*on_write_updating_configuration_callback)(const uint8_t *buf, size_t len);
extern void (*on_write_wifi_config_callback)(const uint8_t *buf, size_t len);
extern void (*on_dynamic_period_change_callback)(uint16_t);

extern esp_bd_addr_t g_peer_bda;
extern bool          g_connected;
extern int8_t        g_rssi_ema;
extern bool          g_rssi_task_running;
extern link_prof_t   g_prof;
extern uint16_t      g_cur_mtu;

extern SemaphoreHandle_t        s_conf_sem;
extern volatile esp_gatt_status_t s_last_conf_status;
extern volatile uint16_t          s_last_conf_handle;

//extern uint16_t notif_ind_cccd_handle;
extern bool     notif_ind_enabled;
extern bool     timer_ind_enabled;
extern bool     device_ind_enabled;
extern bool     bond_ok;

extern volatile bool g_ind_inflight;

extern uint8_t  s_adv_handle;
extern bool     s_ext_adv_started;
extern uint8_t  s_adv_raw[31];
extern uint8_t  s_adv_len;

uint16_t ms_to_conn_int(uint16_t ms);
void     build_adv_data(void);
void     setup_ble_security(void);
esp_err_t read_current_phy(void);
esp_err_t set_phy_masks_(const esp_bd_addr_t addr, esp_ble_gap_phy_mask_t tx_mask,
                         esp_ble_gap_phy_mask_t rx_mask, esp_ble_gap_prefer_phy_options_t opt);

// Event handler prototipleri
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// Bildirim/indication yardımcıları
void     drain_conf_sem(void);
esp_err_t send_notification_char_as_indication(const uint8_t *data, size_t len, uint32_t timeout_ms);
esp_err_t ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length);
void     set_ble_tx_power(void);

void apply_profile(link_prof_t p);
esp_err_t ble_send_auth_message();
bool has_peer(void);

typedef enum {
  STEP_INIT = 0,
  STEP_ADD_AUTH,
  STEP_ADD_RECORDS,
  STEP_ADD_TIMER,
  STEP_ADD_TIMER_CCCD,
  STEP_ADD_MEAS,
  STEP_ADD_NOTIF,
  STEP_ADD_NOTIF_CCCD,
  STEP_ADD_DEVICE,
  STEP_ADD_DEVICE_CCCD,
  STEP_ADD_ACTIVATION,
  STEP_ADD_UPD_RECORDS,
  STEP_ADD_UPD_PASSKEY,
  STEP_ADD_UPD_CONFIG,
  STEP_ADD_UPD_THERAPY_STATE,
  STEP_ADD_RECORDS_FEEDBACK,
  STEP_ADD_WIFI_CONFIG,
  STEP_DONE
} build_step_t;