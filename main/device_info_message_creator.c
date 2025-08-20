#include "device_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "json_encoder.h"
#include "ble_control.h"
#include "esp_mac.h"
#include "therapy_counter.h"
#include "therapy_message_counter.h"
#include <string.h>
#include "message_queue_manager.h"
#include "timer_management.h"
#include "storage/log_writer.h"
#include "ble/ble_state_manager.h"

static const char *TAG = "DeviceInfoMessageCreator";

static void send_device_info(uint16_t passed_seconds) {
    DeviceInfoMessage message;
    //message.current_time; //TODO: set current time when RTC integrated.
    esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    message.passed_seconds = passed_seconds;
    message.last_saved_therapy_id = read_therapy_count() - 1;
    message.message_id = get_message_id();
    char *json_str = encode_device_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }
    size_t len = strlen(json_str);
    send_info_message_to_queue(DEVICE_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    free(json_str);
}

static void on_connect_ble() {
    set_ble_connection_status(true);
    //uint16_t passed_seconds = get_passed_duration();
    //add_notification_log(BLE_CONNECTED, passed_seconds);
    //send_device_info(passed_seconds);
}

void init_device_info_message_creator() {
    register_on_connect_callback(on_connect_ble);
}