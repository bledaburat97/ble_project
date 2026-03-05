#include "../include/ble_internal.h"

static const char *TAG = "BLEGapHandler";

// Pairing/bonding başarı durumunu tutar.
bool bond_ok = false;

// RSSI değerine göre bağlantı profilini seçer.
static link_prof_t choose_profile(int8_t rssi) {
    if (rssi >= -60) return PROF_REALLY_GOOD;
    if (rssi >= -68) return PROF_GOOD;
    if (rssi >= -76) return PROF_FAIR;
    if (rssi >= -84) return PROF_POOR;
    if (rssi >= -92) return PROF_VERY_POOR;
    return PROF_WORST;
}


// GAP event'lerini (ADV, RSSI, PHY, güvenlik) merkezi olarak yönetir.
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {

    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT: {
        // RSSI okundu; kalite profilini güncelle.
        if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            int8_t r = param->read_rssi_cmpl.rssi;
            g_rssi_ema = (int8_t)(0.7f * g_rssi_ema + 0.3f * r);
            static uint8_t tick = 0;
            if ((++tick % 2) == 0) {
                link_prof_t p = choose_profile(g_rssi_ema);
                if (p != g_prof) { 
                    g_prof = p; ESP_LOGW(TAG, "RSSI changed"); 
                    apply_profile(p);
                }
            }
        } else {
            ESP_LOGW(TAG, "RSSI read failed, status=%d", param->read_rssi_cmpl.status);
        }
        break;
    }

    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT: {
        // ADV parametreleri set edildi; raw reklam datasını gönder.
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
        // ADV data yüklendi; reklamı başlat.
        if (param->ext_adv_data_set.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "ext_adv_data_set failed, status=%d", param->ext_adv_data_set.status);
            break;
        }
        esp_ble_gap_ext_adv_t start = {.instance = s_adv_handle, .duration = 0, .max_events = 0};
        esp_err_t err = esp_ble_gap_ext_adv_start(1, &start);
        if (err != ESP_OK) ESP_LOGE(TAG, "ext_adv_start failed: %s", esp_err_to_name(err));
        break;
    }

    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
        // Reklamın başarıyla başlayıp başlamadığını kaydet.
        s_ext_adv_started = (param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS);
        break;

    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
        // Reklam durduruldu; state'i güncelle.
        s_ext_adv_started = false;
        ESP_LOGI(TAG, "Extended advertising stopped (status=%d)", param->ext_adv_stop.status);
        break;

    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        // Bağlantı parametreleri güncellendi.
        ESP_LOGW(TAG, "Conn params updated: status=%d, conn_int=%d, lat=%d, to=%d",
                 param->update_conn_params.status, param->update_conn_params.conn_int,
                 param->update_conn_params.latency, param->update_conn_params.timeout);
        break;

    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        // Link katmanı paket uzunluğu güncellendi.
        ESP_LOGI(TAG, "Pkt length updated: rx=%d tx=%d status=%d",
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len,
                 param->pkt_data_length_cmpl.status);
        break;

    case ESP_GAP_BLE_ADV_TERMINATED_EVT:
        // Reklam sona erdi.
        s_ext_adv_started = false;
        ESP_LOGW(TAG, "Ext adv terminated.");
        break;

    case ESP_GAP_BLE_SET_PREFERRED_PHY_COMPLETE_EVT:
        // PHY tercih isteği tamamlandı.
        ESP_LOGI(TAG, "SET_PREF_PHY complete: status=%d", param->set_perf_phy.status);
        break;

    case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT:
        // Mevcut PHY okundu.
        ESP_LOGI(TAG, "READ_PHY: status=%d tx=%d rx=%d",
                 param->read_phy.status, param->read_phy.tx_phy, param->read_phy.rx_phy);
        break;

    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT: {
        // PHY güncelleme sonucu logu.
        esp_bt_status_t s = param->phy_update.status;
        uint8_t tx = param->phy_update.tx_phy, rx = param->phy_update.rx_phy;
        ESP_LOGW(TAG, "PHY UPDATE: status=%d tx=%u rx=%u", s, tx, rx);
        if (s != ESP_BT_STATUS_SUCCESS) ESP_LOGW(TAG, "PHY update rejected; peer desteklemiyor olabilir.");
        break;
    }

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        // Eşleştirme passkey bildirimi.
        ESP_LOGW(TAG, "PASSKEY: %06lu", param->ble_security.key_notif.passkey);
        break;

    case ESP_GAP_BLE_KEY_EVT:
        // Key exchange aşaması.
        ESP_LOGI(TAG, "KEY_EVT: key type=0x%02x", param->ble_security.ble_key.key_type);
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        // Güvenlik isteğine onay ver.
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        // Pairing/bonding tamamlandı; sonucu kaydet.
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Bond OK");
            bond_ok = true;
        } else {
            ESP_LOGW(TAG, "Bond fail");
            bond_ok = false;
        }
        break;
    default:
        ESP_LOGW(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

// Seçilen bağlantı profiline göre loglar. 
// İleride bağlantıya göre phy ayarı değiştirilebilir mesajlar arasına gecikme ekletilebilir, denendi ama büyük bir gelişme gözlemlenmedi. Tekrar denenebilir.
void apply_profile(link_prof_t p) {
    switch (p) {
        case PROF_REALLY_GOOD: ESP_LOGI(TAG, "PROF_REALLY_GOOD"); break;
        case PROF_GOOD:        ESP_LOGI(TAG, "PROF_GOOD");        break;
        case PROF_FAIR:        ESP_LOGI(TAG, "PROF_FAIR");        break;
        case PROF_POOR:        ESP_LOGI(TAG, "PROF_POOR");        break;
        case PROF_VERY_POOR:   ESP_LOGI(TAG, "PROF_VERY_POOR");   break;
        case PROF_WORST:       ESP_LOGI(TAG, "PROF_WORST");       break;
    }

    //TRY_IN_FUTURE
    /*
        case PROF_GOOD: {
            connection_interval = 24;
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(4 * connection_interval);
            }
            request_conn_interval_ms(connection_interval, 8 * connection_interval);
            set_phy_2m();
            break;
        }
        case PROF_FAIR: {
            connection_interval = 36;
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(5 * connection_interval);
            }
            request_conn_interval_ms(connection_interval, 10 * connection_interval);
            set_phy_2m();
            break;
        }
        case PROF_POOR: {
            connection_interval = 80;
            if(on_dynamic_period_change_callback) {
                on_dynamic_period_change_callback(6 * connection_interval);
            }
            request_conn_interval_ms(connection_interval, 24 * connection_interval);
            set_phy_coded_s2();
            break;
        }
        default: {
            break;
        }
    */
}
