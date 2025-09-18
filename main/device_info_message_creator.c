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
        message.last_saved_therapy_id = read_therapy_count();
    }
    else {
        message.last_saved_therapy_id = read_therapy_count() - 1;
    }
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
    }

}

static void on_connect_ble() {
    set_ble_connection_status(true);
    uint16_t passed_seconds = get_session_passed_seconds();
    add_notification_log(BLE_CONNECTED, passed_seconds);
    send_device_info(passed_seconds);
    restart_duration_update_watchdog_timer();
}

void init_device_info_message_creator() {
    register_on_connect_callback(on_connect_ble);
}