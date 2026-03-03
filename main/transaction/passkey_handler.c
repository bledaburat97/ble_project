#include "passkey_handler.h"

#include "../helper/binary_message_parser.h"

#include "../ble/include/ble_internal.h"
#include "../storage/passkey_partition_manager.h"

#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

static const char *TAG = "PasskeyHandler";

static void change_passkey(uint32_t passkey) {
    if (passkey > 999999u) passkey = 999999u;

    if (!passkey_append(passkey)) {
        ESP_LOGE(TAG, "saving passkey failed.");
    } else {
        ESP_LOGI(TAG, "Passkey is saved.");
    }

    esp_err_t err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set static passkey failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Static passkey set to %lu", passkey);
    }
}

static void on_passkey_updated(const uint8_t *buf, size_t len){
    UpdatePasskeyMessage update_passkey_message;

    if(!decode_update_passkey_message_bin(buf, len, &update_passkey_message)) {
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

void reset_pairing_and_set_default_passkey()
{
    ESP_LOGW(TAG, "Pairing reset started: closing connection, removing bonds, setting passkey=000000");

    // 1) Bağlantı varsa kapat
    if (g_connected) {
        uint16_t conn_id = gl_profile_tab[PROFILE_A_APP_ID].conn_id;
        esp_gatt_if_t gatts_if = gl_profile_tab[PROFILE_A_APP_ID].gatts_if;

        ESP_LOGI(TAG, "Closing current connection (if=%d, conn_id=%u)...", (int)gatts_if, conn_id);
        esp_err_t cerr = esp_ble_gatts_close(gatts_if, conn_id);
        if (cerr != ESP_OK) {
            ESP_LOGW(TAG, "gatts_close failed: %s", esp_err_to_name(cerr));
        }
    }

    // 2) Tüm bond'ları sil
    int dev_num = esp_ble_get_bond_device_num();
    ESP_LOGI(TAG, "Bonded device count: %d", dev_num);

    if (dev_num > 0) {
        esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc((size_t)dev_num * sizeof(esp_ble_bond_dev_t));
        if (!dev_list) {
            ESP_LOGE(TAG, "malloc failed while allocating bond dev list");
        } else {
            int copied = dev_num;
            esp_err_t lerr = esp_ble_get_bond_device_list(&copied, dev_list);
            if (lerr != ESP_OK) {
                ESP_LOGW(TAG, "get_bond_device_list failed: %s", esp_err_to_name(lerr));
            } else {
                for (int i = 0; i < copied; i++) {
                    esp_bd_addr_t *bda = &dev_list[i].bd_addr;

                    esp_err_t rerr = esp_ble_remove_bond_device(*bda);
                    if (rerr == ESP_OK) {
                        ESP_LOGI(TAG, "Removed bond: %02X:%02X:%02X:%02X:%02X:%02X",
                                 (*bda)[0], (*bda)[1], (*bda)[2], (*bda)[3], (*bda)[4], (*bda)[5]);
                    } else {
                        ESP_LOGW(TAG, "remove_bond_device failed: %s", esp_err_to_name(rerr));
                    }
                }
            }
            free(dev_list);
        }
    }

    // 3) peer bda'yı sıfırla
    static const uint8_t zero[6] = {0};
    memcpy(g_peer_bda, zero, sizeof(esp_bd_addr_t));

    // 4) Passkey'i default'a çek (000000)
    change_passkey(0);

    ESP_LOGW(TAG, "Pairing reset done.");
}

void init_passkey_handler() {
    uint32_t passkey = 0;
    init_passkey_partition();

    PasskeyEntry last;
    if (!passkey_read_last(&last)) {
        passkey = 0;
        passkey_append(passkey);
    }
    else{
        passkey = last.passkey;
    }

    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
    register_on_write_updating_passkey_callback(on_passkey_updated);
}