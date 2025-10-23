#include "passkey_handler.h"

#include "../helper/binary_message_parser.h"

#include "../ble/include/ble_internal.h"

#include "../nvs/storage_manager.h"

#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

static const char *TAG = "PasskeyHandler";
static const char *NVS_PASSKEY_KEY = "pass_key";

static void change_passkey(uint32_t passkey) {
    if (passkey > 999999u) passkey = 999999u;

    esp_err_t err = save_parameter_u32(NVS_PASSKEY_KEY, passkey);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving passkey failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Passkey is saved");
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set static passkey failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Static passkey set to %lu", passkey);
    }
}

static void on_passkey_updated(const uint8_t *buf, size_t len){
    UpdatePasskeyMessage update_passkey_message;

    if(!decode_update_passkey_message_bin(buf, &update_passkey_message)) {
        return;
    }


    change_passkey(update_passkey_message.passkey);

    // Bağlantı varsa kopar
    if (g_connected) {
        uint16_t conn_id = gl_profile_tab[PROFILE_A_APP_ID].conn_id;
        esp_gatt_if_t gatts_if = gl_profile_tab[PROFILE_A_APP_ID].gatts_if;
        ESP_LOGI(TAG, "Closing current connection (if=%d, conn_id=%u) to force re-pair...",
                 (int)gatts_if, conn_id);

        esp_err_t err = esp_ble_gatts_close(gatts_if, conn_id);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "gatts_close failed: %s", esp_err_to_name(err));
        }
    }

    // Bu peer’in bond’unu sil
    static const uint8_t zero[6] = {0};
    if (memcmp(g_peer_bda, zero, sizeof(esp_bd_addr_t)) != 0) {
        esp_err_t err = esp_ble_remove_bond_device(g_peer_bda);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Removed bond for peer ");
        } else {
            ESP_LOGW(TAG, "esp_ble_remove_bond_device failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "No peer BDA yet; skip bond removal");
    }

    // Not: Telefon tarafında da “Unpair/Forget” yapmak gerekebilir.
}


void init_passkey_handler() {
    uint32_t passkey = 0;
    if(read_parameter_u32(NVS_PASSKEY_KEY, &passkey) != ESP_OK) {
        passkey = 2345;
        save_parameter_u32(NVS_PASSKEY_KEY, passkey);
    }
    ESP_LOGE(TAG, "passkey: %lu", passkey);
    save_parameter_u32(NVS_PASSKEY_KEY, passkey);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
    register_on_write_updating_passkey_callback(on_passkey_updated);
}