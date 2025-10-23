#include "../include/ble_internal.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BLEPhyManager";

esp_err_t set_phy_masks(const esp_bd_addr_t addr,
                         esp_ble_gap_phy_mask_t tx_mask,
                         esp_ble_gap_phy_mask_t rx_mask,
                         esp_ble_gap_prefer_phy_options_t opt)
{
    if (!g_connected) return ESP_ERR_INVALID_STATE;
    return esp_ble_gap_set_preferred_phy((uint8_t *)addr, 0, tx_mask, rx_mask, opt);
}

esp_err_t set_phy_2m(void)        { ESP_LOGI(TAG, "Try to set phy as 2m");
    return set_phy_masks(g_peer_bda, ESP_BLE_GAP_PHY_2M_PREF_MASK, ESP_BLE_GAP_PHY_2M_PREF_MASK, ESP_BLE_GAP_PHY_OPTIONS_NO_PREF); }

esp_err_t set_phy_1m(void)        { ESP_LOGI(TAG, "Try to set phy as 1m");
    return set_phy_masks(g_peer_bda, ESP_BLE_GAP_PHY_1M_PREF_MASK, ESP_BLE_GAP_PHY_1M_PREF_MASK, ESP_BLE_GAP_PHY_OPTIONS_NO_PREF); }

esp_err_t set_phy_coded_s2(void)  { ESP_LOGI(TAG, "Try to set phy as s2");
    return set_phy_masks(g_peer_bda, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING); }

esp_err_t set_phy_coded_s8(void)  { ESP_LOGI(TAG, "Try to set phy as s8");
    return set_phy_masks(g_peer_bda, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING); }

esp_err_t set_phy_coded_any(void) {
    return set_phy_masks(g_peer_bda, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_CODED_PREF_MASK, ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
}

void request_conn_interval_ms(uint16_t target_ms) {
    if (!has_peer()) { ESP_LOGW(TAG, "No peer BDA yet; skip conn param update"); return; }
    ESP_LOGI(TAG, "Try to set connection interval as %u", target_ms);

    uint16_t min_ms = target_ms * 9 / 10;
    uint16_t max_ms = target_ms * 11 / 10;

    esp_ble_conn_update_params_t prm = {
        .min_int = ms_to_conn_int(min_ms),
        .max_int = ms_to_conn_int(max_ms),
        .latency = 4,
        .timeout = 1500
    };
    memcpy(prm.bda, g_peer_bda, sizeof(esp_bd_addr_t));
    esp_err_t err = esp_ble_gap_update_conn_params(&prm);
    if (err != ESP_OK) ESP_LOGE(TAG, "esp_ble_gap_update_conn_params failed: %s", esp_err_to_name(err));
}

esp_err_t read_current_phy(void) {
    static const uint8_t z[6] = {0};
    if (memcmp(g_peer_bda, z, 6) == 0) {
        ESP_LOGW(TAG, "No peer BDA yet; can't read PHY");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_ble_gap_read_phy(g_peer_bda);
    if (err != ESP_OK) ESP_LOGE(TAG, "esp_ble_gap_read_phy failed: %s", esp_err_to_name(err));
    return err;
}

void set_ble_tx_power(void) {
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P20);
    if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set TX power for connection handle 0: %s", esp_err_to_name(err));
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,   ESP_PWR_LVL_P20); if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set ADV power: %s", esp_err_to_name(err));
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN,  ESP_PWR_LVL_P20); if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set SCAN power: %s", esp_err_to_name(err));
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P20); if (err != ESP_OK) ESP_LOGE(TAG, "Failed to set default TX power: %s", esp_err_to_name(err));
    ESP_LOGI(TAG, "BLE TX power set to maximum level (20 dBm).");
}

