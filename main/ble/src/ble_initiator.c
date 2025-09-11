#include "../include/ble_internal.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLEInitiator";

esp_err_t init_bluetooth(void) {
    esp_err_t ret;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) { ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret)); return ret; }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) { ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret)); return ret; }

    ret = esp_bluedroid_init();
    if (ret) { ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret)); return ret; }

    ret = esp_bluedroid_enable();
    if (ret) { ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret)); return ret; }

    set_ble_tx_power();
    ESP_LOGI(TAG, "Bluetooth initialized successfully");
    return ESP_OK;
}

esp_err_t start_registering_and_advertising(void) {
    setup_ble_security();

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_A_APP_ID));

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }

    esp_ble_gap_ext_adv_params_t p = {
        .type           = ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
        .interval_min   = 0x20,
        .interval_max   = 0x30,
        .channel_map    = ADV_CHNL_ALL,
        .own_addr_type  = BLE_ADDR_TYPE_PUBLIC,
        .filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        .primary_phy    = ESP_BLE_GAP_PHY_1M,
        .secondary_phy  = ESP_BLE_GAP_PHY_1M,
        .max_skip       = 0,
        .tx_power       = 0x7F,
    };

    esp_err_t ret = esp_ble_gap_ext_adv_set_params(s_adv_handle, &p);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ext_adv_set_params failed: %s", esp_err_to_name(ret));
        return ret;
    }
    // Devamı GAP event zincirinde: SET_PARAMS_COMPLETE -> DATA_SET_COMPLETE -> START
    return ret;
}

void register_on_connect_callback(void (*cb)(void))                                                 { on_connect_callback = cb; }
void register_on_disconnect_callback(void (*cb)(void))                                              { on_disconnect_callback = cb; }
void register_on_write_activation_callback(void (*cb)(const uint8_t *buf, size_t len))              { on_write_activation_callback = cb; }
void register_on_write_updating_records_callback(void (*cb)(const uint8_t *buf, size_t len))        { on_write_updating_records_callback = cb; }
//void register_on_write_feedback_callback(void (*cb)(const char*))           { on_write_feedback_callback = cb; }
void register_on_write_updating_therapy_state_callback(void (*cb)(const uint8_t *buf, size_t len))  { on_write_updating_therapy_state_callback = cb; }
void register_on_write_records_feedback_callback(void (*cb)(const uint8_t *buf, size_t len))        { on_write_records_feedback_callback = cb; }
void register_on_write_updating_passkey_callback(void (*cb)(const uint8_t *buf, size_t len))        { on_write_updating_passkey_callback = cb; }
void register_dynamic_period_change_callback(void (*cb)(uint16_t))                                  { on_dynamic_period_change_callback = cb; }