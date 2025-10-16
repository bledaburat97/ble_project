#include "device_info_message_creator.h"

#include "message_encoder.h"
#include "current_therapy_info_manager.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "storage/log_types.h"
#include "therapy_counter.h"
#include "message_queue_manager.h"
#include "timer_management.h"
#include "storage/log_writer.h"
#include "ble/include/ble_connection_state_manager.h"
#include "ble/include/ble_controller.h"
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"

static const char *TAG = "DeviceInfoMessageCreator";

static uint16_t resolve_last_saved_therapy_id(void) {
    uint16_t therapy_count = read_therapy_count();
    if (get_current_therapy_state() == NONE) {
        return therapy_count;
    }
    if (therapy_count == 0) {
        ESP_LOGW(TAG, "Therapy counter returned zero while a session is active.");
        return 0;
    }
    return (uint16_t)(therapy_count - 1u);
}

static void send_device_info(uint16_t passed_seconds) {
    DeviceInfoMessage message = {0};
    //message.current_time; //TODO: set current time when RTC integrated.
    esp_err_t err = esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device MAC: %s", esp_err_to_name(err));
        return;
    }

    message.last_saved_therapy_id = resolve_last_saved_therapy_id();
    ESP_LOGI(TAG, "Sending device info with last saved therapy id: %u", message.last_saved_therapy_id);

    message.passed_seconds = passed_seconds;

    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_device_info_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }
        size_t len = strlen(json_str);
        send_info_message_to_queue(DEVICE_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[DEVICE_INFO_SIZE];
        size_t len = encode_device_info_message_binary(&message, buf);
        send_info_message_to_queue(DEVICE_INFO_MESSAGE, buf, len);
        ESP_LOGI(TAG, "Device info message is sent");
    }
}

static void perform_post_connect_operations(void) {
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = add_notification_log(BLE_CONNECTED, passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist BLE connected notification: %s", esp_err_to_name(err));
    }
    send_device_info(passed_seconds);

    restart_duration_update_watchdog_timer();
}

static void post_connect_sender_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(500)); // 300–800 ms arası idealdir

    perform_post_connect_operations();

    vTaskDelete(NULL);
}

static void on_connect_ble() {
    ESP_LOGI(TAG, "On connect BLE");
    set_ble_connection_status(true);
    if (xTaskCreate(post_connect_sender_task, "post_conn_send", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create post-connect sender task");
        perform_post_connect_operations();
    }
}

void init_device_info_message_creator() {
    register_on_connect_callback(on_connect_ble);
}