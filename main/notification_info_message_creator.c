#include "notification_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "message_encoder.h"
#include "ble/include/ble_controller.h"
#include "timer_management.h"
#include "storage/log_writer.h"
#include "ble/include/ble_connection_state_manager.h"
#include "therapy_message_counter.h"
#include "state_manager.h"
#include <string.h>
#include "message_queue_manager.h"
#include "current_therapy_info_manager.h"

static const char *TAG = "NotificationInfoMessageCreator";

static void on_timer_state_info_feedback_callback() {
    NotificationType helmet_status = get_helmet_state() ? NOTIF_HELMET_ON : NOTIF_HELMET_OFF;     //TODO: tam doğru değil.
    add_and_send_notification_info(helmet_status);
}

void send_notification_info(NotificationType type, uint16_t passed_seconds) {
    NotificationMessage message;
    message.type = type;
    message.passed_seconds = passed_seconds;
    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_notification_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }

        size_t len = strlen(json_str);
        send_info_message_to_queue(NOTIFICATION_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[NOTIFICATION_INFO_SIZE];
        size_t len = encode_notification_message_binary(&message, buf);
        send_info_message_to_queue(NOTIFICATION_INFO_MESSAGE, buf, len);
    }
}

void add_and_send_notification_info(NotificationType notification_type) {
    uint16_t passed_seconds = get_current_therapy_passed_duration();
    add_notification_log(notification_type, passed_seconds);
    send_notification_info(notification_type, passed_seconds);
}

void init_notification_info_message_creator() {
    register_timer_state_info_feedback_callback(on_timer_state_info_feedback_callback);
}
