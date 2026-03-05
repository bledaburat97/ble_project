#include "../include/ble_internal.h"

static const char *TAG = "BLEMessageSender";

// Önceki CONF sinyallerini temizler (indication bekleme öncesi).
void drain_conf_sem(void) {
    if (!s_conf_sem) return;
    while (xSemaphoreTake(s_conf_sem, 0) == pdTRUE) {}
}

// Belirli bir karakteristiğe notify/indicate gönderir.
esp_err_t ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length, bool need_confirm) {
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
        char_handle,
        data_length,
        data,
        need_confirm
    );
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
    }
    return ret;
}

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

// Mesaj tipine göre doğru handle seçip gönderimi yapar.
esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length)
{

    if (message_type == NOTIFICATION_INFO_MESSAGE && !notif_ind_enabled) { 
        ESP_LOGW(TAG, "Client has not enabled indication for Notification Info (CCCD=0x0002 not set)"); 
        return ESP_GATT_INVALID_CFG;
    }
    if (message_type == TIMER_STATE_INFO_MESSAGE && !timer_ind_enabled) { 
        ESP_LOGW(TAG, "Client has not enabled indication for Timer State Info (CCCD=0x0002 not set)"); 
        return ESP_GATT_INVALID_CFG; 
    }
    if (message_type == DEVICE_INFO_MESSAGE && !device_ind_enabled) { 
        ESP_LOGW(TAG, "Client has not enabled indication for Device Info (CCCD=0x0002 not set)"); 
        return ESP_GATT_INVALID_CFG;
    }

    if (message_type == NOTIFICATION_INFO_MESSAGE || message_type == TIMER_STATE_INFO_MESSAGE || message_type == DEVICE_INFO_MESSAGE) {
        return ble_send_message(get_char_handle_by_message_type(message_type), data, data_length, true);
    }
    return ble_send_message(get_char_handle_by_message_type(message_type), data, data_length, false);
    
}
