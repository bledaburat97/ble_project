#include "notification_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "json_encoder.h"
#include "ble_control.h"
#include "timer_management.h"
#include "storage/log_writer.h"
#include "ble/ble_state_manager.h"
#include "therapy_message_counter.h"
#include "state_manager.h"
#include <string.h>
#include "message_queue_manager.h"

static const char *TAG = "NotificationInfoMessageCreator";

static void on_timer_state_info_feedback_callback() {
    NotificationType helmet_status = get_helmet_state() ? NOTIF_HELMET_ON : NOTIF_HELMET_OFF;     //TODO: tam doğru değil.
    add_and_send_notification_info(helmet_status);
}

static void send_notification_info(NotificationType type, uint16_t passed_seconds) {
    NotificationMessage message;
    message.type = type;
    message.passed_seconds = passed_seconds;
    message.message_id = get_message_id();
    char *json_str = encode_notification_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message_to_queue(NOTIFICATION_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    free(json_str);  // cJSON_PrintUnformatted ile heap'e alındığı için temizlenmeli
}

void add_and_send_notification_info(NotificationType notification_type) {
    uint16_t passed_seconds = get_passed_duration();
    add_notification_log(notification_type, passed_seconds);
    send_notification_info(notification_type, passed_seconds);
}

void init_notification_info_message_creator() {
    register_timer_state_info_feedback_callback(on_timer_state_info_feedback_callback);
}
