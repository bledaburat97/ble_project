#include "../include/ble_internal.h"

static const char *TAG = "BLEMessageSender";

void drain_conf_sem(void) {
    if (!s_conf_sem) return;
    while (xSemaphoreTake(s_conf_sem, 0) == pdTRUE) {}
}

esp_err_t ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length) {
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
        char_handle,
        data_length,
        data,
        true
    );
    if (ret != ESP_OK) ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
    else               ESP_LOGI(TAG, "Successfully sent.");
    return ret;
}

esp_err_t send_notification_char_as_indication(const uint8_t *data, size_t len, uint32_t timeout_ms) {
    /*if (!s_conf_sem) { ESP_LOGE(TAG, "s_conf_sem is null"); return ESP_FAIL; }
    if (g_ind_inflight) { ESP_LOGE(TAG, "another message is in flight."); return ESP_ERR_INVALID_STATE; }

    drain_conf_sem();
    g_ind_inflight = true;
*/
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
        gl_profile_tab[PROFILE_A_APP_ID].notification_handle,
        len, (uint8_t*)data,
        true
    );

    if (ret != ESP_OK) ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
    else               ESP_LOGI(TAG, "Successfully sent.");
    return ret;

/*
    if (ret != ESP_OK) { g_ind_inflight = false; ESP_LOGE(TAG, "ret is not ok: %s", esp_err_to_name(ret)); return ret; }
    if (xSemaphoreTake(s_conf_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) { g_ind_inflight = false; ESP_LOGE(TAG, "Indication CONF timeout"); return ESP_ERR_TIMEOUT; }

    g_ind_inflight = false;
    return (s_last_conf_status == ESP_GATT_OK) ? ESP_OK : ESP_FAIL;
    */
}

// Bu fonksiyon public API’de (ble_control.h), ama implementasyonu burada:
static uint16_t get_char_handle_by_message_type(MessageType type) {
    switch (type) {
        case RECORDS_INFO_MESSAGE:     return gl_profile_tab[PROFILE_A_APP_ID].records_handle;
        case TIMER_STATE_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].timer_state_handle;
        case MEASUREMENT_INFO_MESSAGE: return gl_profile_tab[PROFILE_A_APP_ID].measurement_handle;
        case NOTIFICATION_INFO_MESSAGE:return gl_profile_tab[PROFILE_A_APP_ID].notification_handle;
        case DEVICE_INFO_MESSAGE:      return gl_profile_tab[PROFILE_A_APP_ID].device_handle;
        default: return 0;
    }
}

esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length)
{

    if (message_type == NOTIFICATION_INFO_MESSAGE && !notif_ind_enabled) { ESP_LOGW(TAG, "Client has not enabled indication for Notification Info (CCCD=0x0002 not set)"); return ESP_GATT_INVALID_CFG; }
    if (message_type == TIMER_STATE_INFO_MESSAGE && !timer_ind_enabled) { ESP_LOGW(TAG, "Client has not enabled indication for Timer State Info (CCCD=0x0002 not set)"); return ESP_GATT_INVALID_CFG; }
    if (message_type == DEVICE_INFO_MESSAGE && !device_ind_enabled) { ESP_LOGW(TAG, "Client has not enabled indication for Device Info (CCCD=0x0002 not set)"); return ESP_GATT_INVALID_CFG; }

    if (message_type == NOTIFICATION_INFO_MESSAGE) {
        return send_notification_char_as_indication(data, data_length, 10000);
    }
    return ble_send_message(get_char_handle_by_message_type(message_type), data, data_length);
}


esp_err_t ble_send_auth_message() {
    uint8_t data = 0x01;
    return ble_send_message(gl_profile_tab[PROFILE_A_APP_ID].auth_handle, &data, 1);
}
