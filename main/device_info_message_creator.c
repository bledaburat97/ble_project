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


static const char *TAG = "DeviceInfoMessageCreator";

static void send_device_info(uint16_t passed_seconds) {
    DeviceInfoMessage message;
    //message.current_time; //TODO: set current time when RTC integrated.
    esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    if(get_current_therapy_state() == NONE) {
        message.last_saved_therapy_id = read_therapy_count(); //TODO: direkt therapy count değil de maks therapy_count'u geçerse sıkıntı olur.
    }
    else {
        message.last_saved_therapy_id = read_therapy_count() - 1;
    }
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

static void post_connect_sender_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(500)); // 300–800 ms arası idealdir

    uint16_t passed_seconds = get_session_passed_seconds();
    add_notification_log(BLE_CONNECTED, passed_seconds);

    send_device_info(passed_seconds);

    restart_duration_update_watchdog_timer();

    vTaskDelete(NULL);
}

static void on_connect_ble() {
    ESP_LOGI(TAG, "On connect BLE");
    set_ble_connection_status(true);
    xTaskCreate(post_connect_sender_task, "post_conn_send", 2048, NULL, 5, NULL);
}

void init_device_info_message_creator() {
    register_on_connect_callback(on_connect_ble);
}