#include "../include/ble_internal.h"
#include "matching_message_encoder.h"

static const char *TAG = "BLEProfileManager";

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
                        //ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
                      ESP_GATT_PERM_READ_ENC_MITM | ESP_GATT_PERM_WRITE_ENC_MITM, NULL, NULL);
    if (err != ESP_OK) ESP_LOGE(TAG, "add CCCD failed: %s", esp_err_to_name(err));
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

void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
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
        
        if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_AUTH) {
            gl_profile_tab[PROFILE_A_APP_ID].auth_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Auth Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_RECORDS) {
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
        /*
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_FEEDBACK) {
            gl_profile_tab[PROFILE_A_APP_ID].feedback_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Feedback Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        */
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_UPDATING_THERAPY_STATE) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Updating Therapy State Info Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_UPDATING_PASSKEY) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_passkey_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Updating Passkey Characteristic Handle: %d", param->add_char.attr_handle);
        }
        else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_UPDATING_CONFIG) {
            gl_profile_tab[PROFILE_A_APP_ID].updating_configuration_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Updating Configuration Characteristic Handle: %d", param->add_char.attr_handle);
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
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_AUTH},
                               ESP_GATT_PERM_READ_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding auth info char failed, error code =%x", add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_RECORDS},
                               ESP_GATT_PERM_READ_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding records info char failed, error code =%x", add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_TIMER_STATE},
                               ESP_GATT_PERM_READ_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding starting info char failed, error code =%x", add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_MEASUREMENT},
                               ESP_GATT_PERM_READ_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding measurement info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_NOTIFICATION},
                               ESP_GATT_PERM_READ_ENC_MITM,
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
                               ESP_GATT_PERM_READ_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_INDICATE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding device info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_ACTIVATION},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding activation info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_RECORDS},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding updating records info char failed, error code =%x",add_char_ret);
        }

        /*
        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_FEEDBACK},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding feedback info char failed, error code =%x",add_char_ret);
        }
        */
        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_PASSKEY},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "updating passkey char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_CONFIG},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "updating configuration char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_UPDATING_THERAPY_STATE},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "adding updating therapy state info char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_RECORDS_FEEDBACK},
                               ESP_GATT_PERM_WRITE_ENC_MITM,
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
            if(param->write.value != NULL && param->write.len == 8) {
                //char json_str[MAX_JSON_STRING_SIZE];
                //memcpy(json_str, param->write.value, param->write.len);
                //json_str[param->write.len] = '\0';
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
                //char json_str[MAX_JSON_STRING_SIZE];
                //memcpy(json_str, param->write.value, param->write.len);
                //json_str[param->write.len] = '\0';
                if (on_write_updating_records_callback) {
                    on_write_updating_records_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        }
        /*
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
        */
        else if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].updating_therapy_state_handle) {
            if(param->write.value != NULL && param->write.len == 1) {
                //char json_str[MAX_JSON_STRING_SIZE];
                //memcpy(json_str, param->write.value, param->write.len);
                //json_str[param->write.len] = '\0';
                //ESP_LOGI(TAG, "Write therapy state as %s", json_str);

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
                //char json_str[MAX_JSON_STRING_SIZE];
                //memcpy(json_str, param->write.value, param->write.len);
                //json_str[param->write.len] = '\0';
                //ESP_LOGI(TAG, "Write therapy state as %s", json_str);

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
            if(param->write.value != NULL && param->write.len == 2) {
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
                //char json_str[MAX_JSON_STRING_SIZE];
                //memcpy(json_str, param->write.value, param->write.len);
                //json_str[param->write.len] = '\0';
                
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
        if (on_disconnect_callback) on_disconnect_callback();

        esp_ble_gap_ext_adv_t start = { .instance = s_adv_handle, .duration = 0, .max_events = 0 };
        err = esp_ble_gap_ext_adv_start(1, &start);
        if (err != ESP_OK) ESP_LOGE(TAG, "ext_adv_start (re) failed: %s", esp_err_to_name(err));

        if (s_conf_sem) { vSemaphoreDelete(s_conf_sem); s_conf_sem = NULL; }
        notif_ind_enabled = false;
        g_ind_inflight = false;
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

        esp_ble_gap_set_pkt_data_len(param->connect.remote_bda, 251);

        read_current_phy();
        ble_send_auth_message();
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