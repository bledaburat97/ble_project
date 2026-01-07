#include "notification_info_message_creator.h"

#include "message_queue_manager.h"

#include "../storage/log_types.h"
#include "../storage/log_writer.h"

#include "../helper/binary_message_encoder.h"

#include "../ble/include/ble_controller.h"
#include "../ble/include/ble_connection_state_manager.h"

#include "../manager/session_timer_manager.h"
#include "../manager/state_controller.h"
#include "../manager/current_therapy_state_manager.h"
#include "../manager/timer_info_getter.h"

#include "../i2c/proximity/proximity_sensor_controller.h"

#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"

static const char *TAG = "NotificationInfoMessageCreator";

static void on_timer_state_info_feedback_callback() {
    NotificationType helmet_status = get_helmet_state() ? NOTIF_HELMET_ON : NOTIF_HELMET_OFF;     //TODO: tam doğru değil.
    add_and_send_notification_info(helmet_status);
}

static void on_passed_duration_update() {
    if(get_device_state() == STATE_ACTIVE) {
        uint16_t passed_seconds = get_session_passed_seconds();
        esp_err_t err = add_notification_log(PASSED_DURATION_UPDATED, passed_seconds);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to persist notification log: %s", esp_err_to_name(err));
        }
    }
}

static void on_disconnect_ble() {
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = add_notification_log(BLE_DISCONNECTED, passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist BLE disconnected notification: %s", esp_err_to_name(err));
    }
    restart_duration_update_watchdog_timer();
}

void send_notification_info(NotificationType type, uint16_t passed_seconds) {
    NotificationMessage message = {
        .type = type,
        .passed_seconds = passed_seconds
    };

    uint8_t buf[NOTIFICATION_INFO_SIZE];
    size_t len = encode_notification_message_binary(&message, buf);
    send_info_message_to_queue(NOTIFICATION_INFO_MESSAGE, buf, len);
}

void add_and_send_notification_info(NotificationType notification_type) {
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = add_notification_log(notification_type, passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist notification log: %s", esp_err_to_name(err));
    }
    send_notification_info(notification_type, passed_seconds);
    restart_duration_update_watchdog_timer();
}

void init_notification_info_message_creator() {
    register_timer_state_info_feedback_callback(on_timer_state_info_feedback_callback);
    register_passed_duration_update(on_passed_duration_update);
    register_on_disconnect_callback(on_disconnect_ble);
}
