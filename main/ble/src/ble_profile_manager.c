#include "../include/ble_internal.h"
#include "../include/ble_connection_state_manager.h"

#include "../../transaction/matching_message_encoder.h"

static const char *TAG = "BLEProfileManager";
static build_step_t s_build = STEP_INIT;
static bool s_service_started = false;

static uint8_t s_ccc_val[2] = {0x00, 0x00};
static esp_attr_value_t s_ccc_attr = {
    .attr_max_len = 2,
    .attr_len     = 2,
    .attr_value   = s_ccc_val,
};

// --- Her char için ayrı CCCD handle ve enable flag
static uint16_t notif_ind_cccd_handle = 0;
static uint16_t timer_ind_cccd_handle = 0;
static uint16_t device_ind_cccd_handle = 0;

bool notif_ind_enabled  = false;
bool timer_ind_enabled  = false;
bool device_ind_enabled = false;

static esp_err_t add_char_16(uint16_t uuid16, uint16_t perms, uint8_t props) {
  esp_bt_uuid_t u = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = uuid16}};
  return esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &u, perms, props, NULL, NULL);
}

static esp_err_t add_cccd_for_last_char(void) {
  esp_bt_uuid_t cccd_uuid = {.len = ESP_UUID_LEN_16, .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}};
  return esp_ble_gatts_add_char_descr(
      gl_profile_tab[PROFILE_A_APP_ID].service_handle,
      &cccd_uuid,
      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      &s_ccc_attr,
      NULL
  );
}

static void run_build_step(void) {
  esp_err_t e = ESP_OK;

  switch (s_build) {
    case STEP_ADD_AUTH:
      e = add_char_16(GATTS_CHAR_UUID_AUTH,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
      ESP_LOGI(TAG, "ADD AUTH char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_RECORDS:
      e = add_char_16(GATTS_CHAR_UUID_RECORDS,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
      ESP_LOGI(TAG, "ADD RECORDS char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_TIMER:
      e = add_char_16(GATTS_CHAR_UUID_TIMER_STATE,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE);
      ESP_LOGI(TAG, "ADD TIMER_STATE char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_TIMER_CCCD:
      e = add_cccd_for_last_char();
      ESP_LOGI(TAG, "ADD TIMER_STATE CCCD -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_MEAS:
      e = add_char_16(GATTS_CHAR_UUID_MEASUREMENT,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
      ESP_LOGI(TAG, "ADD MEASUREMENT char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_NOTIF:
      e = add_char_16(GATTS_CHAR_UUID_NOTIFICATION,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE);
      ESP_LOGI(TAG, "ADD NOTIFICATION char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_NOTIF_CCCD:
      e = add_cccd_for_last_char();
      ESP_LOGI(TAG, "ADD NOTIFICATION CCCD -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_DEVICE:
      e = add_char_16(GATTS_CHAR_UUID_DEVICE,
                      ESP_GATT_PERM_READ_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE);
      ESP_LOGI(TAG, "ADD DEVICE char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_DEVICE_CCCD:
      e = add_cccd_for_last_char();
      ESP_LOGI(TAG, "ADD DEVICE CCCD -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_ACTIVATION:
      e = add_char_16(GATTS_CHAR_UUID_ACTIVATION,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD ACTIVATION char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_UPD_RECORDS:
      e = add_char_16(GATTS_CHAR_UUID_UPDATING_RECORDS,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD UPD_RECORDS char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_UPD_PASSKEY:
      e = add_char_16(GATTS_CHAR_UUID_UPDATING_PASSKEY,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD UPD_PASSKEY char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_UPD_CONFIG:
      e = add_char_16(GATTS_CHAR_UUID_UPDATING_CONFIG,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD UPD_CONFIG char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_UPD_THERAPY_STATE:
      e = add_char_16(GATTS_CHAR_UUID_UPDATING_THERAPY_STATE,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD UPD_THERAPY_STATE char -> %s", esp_err_to_name(e));
      break;

    case STEP_ADD_RECORDS_FEEDBACK:
      e = add_char_16(GATTS_CHAR_UUID_RECORDS_FEEDBACK,
                      ESP_GATT_PERM_WRITE_ENC_MITM,
                      ESP_GATT_CHAR_PROP_BIT_WRITE);
      ESP_LOGI(TAG, "ADD RECORDS_FEEDBACK char -> %s", esp_err_to_name(e));
      break;

    case STEP_DONE:
      if (!s_service_started) {
        esp_err_t se = esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);
        s_service_started = (se == ESP_OK);
        ESP_LOGI(TAG, "SERVICE START -> %s", esp_err_to_name(se));
      }
      return;

    default:
      return;
  }

  if (e != ESP_OK) {
    ESP_LOGE(TAG, "run_build_step error: %s (step=%d)", esp_err_to_name(e), (int)s_build);
    // Hata olsa bile ilerlemeyi durdurmak istemiyorsan burada da ilerletebilirsin.
  }
}



static inline void parse_cccd_write(uint16_t descr_handle, const uint8_t *val, size_t len) {
    if (len < 2) return;
    uint16_t cfg = val[0] | (val[1] << 8); // 0x0001=NOTIFY, 0x0002=INDICATE

    if (descr_handle == notif_ind_cccd_handle) {
        notif_ind_enabled = (cfg & 0x0002) != 0;
        ESP_LOGI(TAG, "Notification-char IND = %s", notif_ind_enabled ? "ENABLED" : "DISABLED");
    } else if (descr_handle == timer_ind_cccd_handle) {
        timer_ind_enabled = (cfg & 0x0002) != 0;
        ESP_LOGI(TAG, "Timer-state IND = %s", timer_ind_enabled ? "ENABLED" : "DISABLED");
    } else if (descr_handle == device_ind_cccd_handle) {
        device_ind_enabled = (cfg & 0x0002) != 0;
        ESP_LOGI(TAG, "Device-info IND = %s", device_ind_enabled ? "ENABLED" : "DISABLED");
    }

    if (notif_ind_enabled && timer_ind_enabled && device_ind_enabled) {
        if(on_connect_callback) {
            on_connect_callback();
        }
    }
}

bool has_peer(void) {
    static const uint8_t zero[6] = {0};
    return memcmp(g_peer_bda, zero, 6) != 0;
}

static void rssi_poll_task(void *arg) {
    g_rssi_task_running = true;
    while (g_rssi_task_running) {
        if (has_peer()) esp_ble_gap_read_rssi(g_peer_bda);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

static void apply_prof_task(void *arg){
  apply_profile(g_prof);
  vTaskDelete(NULL);
}

void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATT profile registered, app_id: %d", param->reg.app_id);
        esp_err_t err = esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = 0xFFF0

        }, 64); // Handle sayısı
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Service creation failed, error code: 0x%X", err);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:

        if (param->add_char.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "add_char failed: uuid=0x%04X status=%d",
                    param->add_char.char_uuid.uuid.uuid16, param->add_char.status);
        }
        else {
            uint16_t uuid = param->add_char.char_uuid.uuid.uuid16;
            uint16_t h    = param->add_char.attr_handle;
            ESP_LOGI(TAG, "ADD_CHAR OK: uuid=0x%04X handle=%u", uuid, h);

            if (uuid == GATTS_CHAR_UUID_AUTH) {
            gl_profile_tab[PROFILE_A_APP_ID].auth_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_RECORDS) {
            gl_profile_tab[PROFILE_A_APP_ID].records_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_TIMER_STATE) {
            gl_profile_tab[PROFILE_A_APP_ID].timer_state_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_MEASUREMENT) {
            gl_profile_tab[PROFILE_A_APP_ID].measurement_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_NOTIFICATION) {
            gl_profile_tab[PROFILE_A_APP_ID].notification_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_DEVICE) {
            gl_profile_tab[PROFILE_A_APP_ID].device_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_ACTIVATION) {
            gl_profile_tab[PROFILE_A_APP_ID].activation_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_UPDATING_RECORDS) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_records_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_UPDATING_PASSKEY) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_passkey_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_UPDATING_CONFIG) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_configuration_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_UPDATING_THERAPY_STATE) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle = h;
            } else if (uuid == GATTS_CHAR_UUID_RECORDS_FEEDBACK) {
            gl_profile_tab[PROFILE_A_APP_ID].records_feedback_handle = h;
            }
        }

        switch (s_build) {
            case STEP_ADD_AUTH:              s_build = STEP_ADD_RECORDS;          break;
            case STEP_ADD_RECORDS:           s_build = STEP_ADD_TIMER;            break;
            case STEP_ADD_TIMER:             s_build = STEP_ADD_TIMER_CCCD;       break;
            case STEP_ADD_TIMER_CCCD:        /* buraya char event düşmez */       break;
            case STEP_ADD_MEAS:              s_build = STEP_ADD_NOTIF;            break;
            case STEP_ADD_NOTIF:             s_build = STEP_ADD_NOTIF_CCCD;       break;
            case STEP_ADD_NOTIF_CCCD:        /* buraya char event düşmez */       break;
            case STEP_ADD_DEVICE:            s_build = STEP_ADD_DEVICE_CCCD;      break;
            case STEP_ADD_DEVICE_CCCD:       /* buraya char event düşmez */       break;
            case STEP_ADD_ACTIVATION:        s_build = STEP_ADD_UPD_RECORDS;      break;
            case STEP_ADD_UPD_RECORDS:       s_build = STEP_ADD_UPD_PASSKEY;      break;
            case STEP_ADD_UPD_PASSKEY:       s_build = STEP_ADD_UPD_CONFIG;       break;
            case STEP_ADD_UPD_CONFIG:        s_build = STEP_ADD_UPD_THERAPY_STATE;break;
            case STEP_ADD_UPD_THERAPY_STATE: s_build = STEP_ADD_RECORDS_FEEDBACK; break;
            case STEP_ADD_RECORDS_FEEDBACK:  s_build = STEP_DONE;                 break;
            default: break;
        }

        run_build_step();
        break;

    case ESP_GATTS_CREATE_EVT:
      
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
    
        s_build = STEP_ADD_AUTH;
        run_build_step();
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "WRITE handle=0x%04X len=%u v[0..1]=%02X %02X",
                param->write.handle, param->write.len,
                param->write.len>0?param->write.value[0]:0,
                param->write.len>1?param->write.value[1]:0);

        if (param->write.handle == notif_ind_cccd_handle ||
                param->write.handle == timer_ind_cccd_handle ||
                param->write.handle == device_ind_cccd_handle) {
            parse_cccd_write(param->write.handle, param->write.value, param->write.len);
            ESP_LOGW(TAG, "CCCD is parsed");
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }

        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].activation_handle) {
            if(param->write.value != NULL && param->write.len == 8) {
                if (on_write_activation_callback) {
                    on_write_activation_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_records_handle) {
            if(param->write.value != NULL && param->write.len == 2) {
                if (on_write_updating_records_callback) {
                    on_write_updating_records_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }

        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle) {
            if(param->write.value != NULL && param->write.len == 1) {
                if (on_write_updating_therapy_state_callback) {
                    on_write_updating_therapy_state_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_passkey_handle) {
            if(param->write.value != NULL && param->write.len == 2) {
                if (on_write_updating_passkey_callback) {
                    on_write_updating_passkey_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_configuration_handle) {
            if(param->write.value != NULL && param->write.len == 8) {
                if (on_write_updating_configuration_callback) {
                    on_write_updating_configuration_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].records_feedback_handle) {
            if(param->write.value != NULL && param->write.len == 3) {
                if (on_write_records_feedback_callback) {
                    on_write_records_feedback_callback(param->write.value, param->write.len);
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
        set_ble_connection_status(false);
        if (on_disconnect_callback) on_disconnect_callback();

        esp_ble_gap_ext_adv_t start = { .instance = s_adv_handle, .duration = 0, .max_events = 0 };
        esp_err_t err2 = esp_ble_gap_ext_adv_start(1, &start);
        if (err2 != ESP_OK) ESP_LOGE(TAG, "ext_adv_start (re) failed: %s", esp_err_to_name(err2));

        if (s_conf_sem) { vSemaphoreDelete(s_conf_sem); s_conf_sem = NULL; }
        notif_ind_enabled = false;
        timer_ind_enabled = false;
        device_ind_enabled = false;

        g_ind_inflight = false;
        bond_ok = false;
        break;
    
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected");
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
        memcpy(g_peer_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        g_connected = true;

        esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);

        g_prof = PROF_GOOD;
        // RSSI poll’u başlat
        xTaskCreate(rssi_poll_task, "rssi_poll", 2048, NULL, 5, NULL);

        esp_ble_gap_set_pkt_data_len(param->connect.remote_bda, 251);

        read_current_phy();
        //ble_send_auth_message();
        set_ble_connection_status(true);

        //if (on_connect_callback) on_connect_callback();
        if (!s_conf_sem) {
            s_conf_sem = xSemaphoreCreateBinary();
        }
        notif_ind_enabled = false;
        timer_ind_enabled = false;
        device_ind_enabled = false;
        bond_ok = false;
        xTaskCreate(apply_prof_task, "apply_prof", 2048, NULL, 5, NULL);
        break;
    case ESP_GATTS_CONF_EVT:
        const bool is_notif = (param->conf.handle == gl_profile_tab[PROFILE_A_APP_ID].notification_handle);
        const bool is_timer = (param->conf.handle == gl_profile_tab[PROFILE_A_APP_ID].timer_state_handle);
        const bool is_device = (param->conf.handle == gl_profile_tab[PROFILE_A_APP_ID].device_handle);

        s_last_conf_status = param->conf.status;
        s_last_conf_handle = param->conf.handle;

        if(is_notif || is_timer || is_device) {
            if(param->conf.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Indication CONF OK: handle=%u", param->conf.handle);
            }
            else {
                ESP_LOGW(TAG, "Indication CONF failed: status=%d handle=%u",
                    param->conf.status, param->conf.handle);
            }
        }

        if (s_conf_sem) xSemaphoreGive(s_conf_sem);
        break;
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        esp_gatt_status_t st = param->add_char_descr.status;
        uint16_t dh = param->add_char_descr.attr_handle;

        ESP_LOGI(TAG, "ADD_DESCR (CCCD?) status=%d handle=%u", st, dh);

        if (st != ESP_GATT_OK) {
            ESP_LOGE(TAG, "ADD_DESCR failed, step=%d", (int)s_build);
        }
        else {
            if (s_build == STEP_ADD_TIMER_CCCD) {
            timer_ind_cccd_handle = dh;
            ESP_LOGI(TAG, "TIMER CCCD handle=%u", dh);
            } else if (s_build == STEP_ADD_NOTIF_CCCD) {
            notif_ind_cccd_handle = dh;
            ESP_LOGI(TAG, "NOTIF CCCD handle=%u", dh);
            } else if (s_build == STEP_ADD_DEVICE_CCCD) {
            device_ind_cccd_handle = dh;
            ESP_LOGI(TAG, "DEVICE CCCD handle=%u", dh);
            }
        }

        esp_bt_uuid_t *u = &param->add_char_descr.descr_uuid;

        ESP_LOGI(TAG, "ADD_DESCR: handle=%u uuid16=0x%04X status=%d",
                param->add_char_descr.attr_handle,
                (u->len==ESP_UUID_LEN_16 ? u->uuid.uuid16 : 0xFFFF), param->add_char_descr.status);
        

        switch (s_build) {
            case STEP_ADD_TIMER_CCCD:  s_build = STEP_ADD_MEAS;              break;
            case STEP_ADD_NOTIF_CCCD:  s_build = STEP_ADD_DEVICE;            break;
            case STEP_ADD_DEVICE_CCCD: s_build = STEP_ADD_ACTIVATION;        break;
            default: break;
        }

        run_build_step(); // sonraki adımı çalıştır
        break;
    case ESP_GATTS_MTU_EVT: {
        g_cur_mtu = param->mtu.mtu;
        ESP_LOGW(TAG, "MTU updated: %u", g_cur_mtu);
        fragments_set_capacity_from_mtu(g_cur_mtu);
        break;
    }
    default:
        break;
    }
}

bool ble_wait_for_indication_conf(esp_gatt_status_t *out_status, uint32_t timeout_ms){
    if (!s_conf_sem) return false;
    if (xSemaphoreTake(s_conf_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        if (out_status) *out_status = s_last_conf_status;
        return true;
    }
    return false; // timeout
}