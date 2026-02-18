#include "../include/ble_internal.h"

#include "../../nvs/storage_manager.h"

// BLE güvenlik parametrelerini (bonding, MITM, IO cap) ayarlar.
void setup_ble_security(void) {
    uint8_t auth_req = ESP_LE_AUTH_REQ_SC_BOND | ESP_LE_AUTH_REQ_MITM; // SC+MITM+Bond
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));

    uint8_t only_accept = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &only_accept, sizeof(only_accept));

    uint8_t iocap = ESP_IO_CAP_OUT; // LED/ekran varsa OUT; tuş takımı varsa KEYBOARD imkanına göre OUT/IN/KEYBOARD/DISPLAY/NO_INPUT_NO_OUTPUT
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));

    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));

    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t resp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,  &resp_key, sizeof(resp_key));
}
