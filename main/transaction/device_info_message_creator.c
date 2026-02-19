#include "device_info_message_creator.h"

#include "message_queue_manager.h"

#include "../helper/binary_message_encoder.h"

#include "../storage/log_types.h"

#include "../ble/include/ble_connection_state_manager.h"
#include "../ble/include/ble_controller.h"

#include "../manager/timer_info_getter.h"
#include "../manager/current_therapy_state_manager.h"
#include "../manager/therapy_id_manager.h"
#include "../manager/session_timer_getter.h"
#include "../manager/message_saver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_log.h"

static const char *TAG = "DeviceInfoMessageCreator";

// Bağlantı sonrası cihaz kimlik bilgilerini gönderir.
static void send_device_info(uint16_t passed_seconds) {
    DeviceInfoMessage message = {0};
    //message.current_time; //TODO: set current time when RTC integrated.
    esp_err_t err = esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device MAC: %s", esp_err_to_name(err));
        return;
    }
    message.passed_seconds = passed_seconds;
    message.last_saved_therapy_id = get_last_completed_therapy_id();
    ESP_LOGI(TAG, "Sending device info with last saved therapy id: %u", message.last_saved_therapy_id);
    uint8_t buf[DEVICE_INFO_SIZE];
    size_t len = encode_device_info_message_binary(&message, buf);
    send_info_message_to_queue(DEVICE_INFO_MESSAGE, buf, len);
}

// İlk senkron için BLE_CONNECTED log'u + device info gönderimi yapar.
static void perform_post_connect_operations(void) {
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = save_log(BLE_CONNECTED, NULL, 0, passed_seconds);
    ESP_LOGI(TAG, "BLE_CONNECTED");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist BLE connected notification: %s", esp_err_to_name(err));
    }
    send_device_info(passed_seconds);

    restart_duration_update_watchdog_timer();
}

// CCCD'ler hazır olunca ilk senkronu tetikler.
static void on_connect_ble() {
    ESP_LOGI(TAG, "On connect BLE");
    vTaskDelay(pdMS_TO_TICKS(500));
    perform_post_connect_operations();
}

void init_device_info_message_creator() {
    register_on_connect_callback(on_connect_ble);
}
