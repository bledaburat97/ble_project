#include "../include/ble_internal.h"

static const char *TAG = "BLEGattsHandler";

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                         esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
            esp_ble_gap_set_device_name("BLE-DA-FLEX");
            ESP_LOGI(TAG, "GATTS_REG_EVT OK app_id=%04x", param->reg.app_id);
        } else {
            ESP_LOGE(TAG, "GATTS_REG_EVT FAIL app_id=%04x status=%d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    for (int idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
        }
    }
}