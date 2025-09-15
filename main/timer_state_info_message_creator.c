#include "timer_state_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "timer_management.h"
#include "message_encoder.h"
#include "storage/log_writer.h"
#include "message_queue_manager.h"
#include "state_manager.h"
#include "therapy_message_counter.h"
#include <string.h>
#include "message_queue_manager.h"
#include "current_therapy_info_manager.h"
#include "timer_management.h"

static const char *TAG = "TimerStateInfoMessageCreator";
static void (*active_or_paused_therapy_callback)() = NULL;

static void on_device_info_feedback_callback() {
    TimerStateInfoMessage message;
    DeviceState device_state = get_device_state();
    CurrentTherapyState currentTherapyState = get_current_therapy_state();
    if(device_state == STATE_ACTIVE) {
        if(currentTherapyState != ACTIVE) {
            ESP_LOGE(TAG, "Current therapy state is not set correctly");
            return;
        }
        message.type = TIMER_STATE_ACTIVE_THERAPY;
        message.duration = get_current_therapy_duration();
        message.therapy_id = get_current_therapy_id();
        message.remaining_seconds = get_therapy_remaining_seconds();
        message.therapy_passed_seconds = get_current_therapy_passed_duration();
    }
    else if(device_state == STATE_INACTIVE) {
        if(currentTherapyState == PAUSED) {
            message.type = TIMER_STATE_PAUSED_THERAPY;
            message.therapy_id = get_current_therapy_id();
            message.duration = get_current_therapy_duration();
            message.remaining_seconds = get_inactivity_remaining_seconds();
            message.therapy_passed_seconds = get_current_therapy_passed_duration();
        } 
        else if(currentTherapyState == NONE) {
            message.type = TIMER_STATE_INACTIVE;
            message.therapy_id = 0;
            message.duration = get_inactivity_duration();
            message.remaining_seconds = get_inactivity_remaining_seconds();
            message.therapy_passed_seconds = get_current_therapy_passed_duration();
        }
        else {
            ESP_LOGE(TAG, "Current therapy state is not set correctly");
            return;
        }        
    }
    else if(device_state == STATE_TEMPERATURE_ALERT) {
        if (currentTherapyState == PAUSED) {
            message.therapy_id = get_current_therapy_id();
            message.therapy_passed_seconds = get_current_therapy_passed_duration();
        }
        else if(currentTherapyState == NONE) {
            message.therapy_id = 0;
            message.therapy_passed_seconds = 0;
        }
        else {
            ESP_LOGE(TAG, "Current therapy state is not set correctly for the temp alert.");
            return;
        }

        message.type = TIMER_STATE_HIGH_TEMP_ALERT_1; //TODO: hangi sensörde hata varsa o olacak.
        message.duration = get_alert_duration();
        message.remaining_seconds = get_alert_remaining_seconds();
    }
    else if(device_state == STATE_HUMIDITY_ALERT) {
        if (currentTherapyState == PAUSED) {
            message.therapy_id = get_current_therapy_id();
            message.therapy_passed_seconds = get_current_therapy_passed_duration();
        }
        else if(currentTherapyState == NONE) {
            message.therapy_id = 0;
            message.therapy_passed_seconds = 0;
        }
        else {
            ESP_LOGE(TAG, "Current therapy state is not set correctly for the humidity alert.");
            return;
        }
        message.type = TIMER_STATE_LOW_HUM_ALERT; //TODO: hangi hata varsa o olacak.
        message.duration = get_alert_duration();
        message.remaining_seconds = get_alert_remaining_seconds();
    }
    else {
        ESP_LOGE(TAG, "Device state: %u is not correct.", device_state);
        return;
    }

    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_timer_state_info_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }

        size_t len = strlen(json_str);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[TIMER_STATE_INFO_SIZE];
        size_t len = encode_timer_state_info_message_binary(&message, buf);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, buf, len);
    }

    if(message.type == TIMER_STATE_ACTIVE_THERAPY || message.type == TIMER_STATE_PAUSED_THERAPY) {
        if(active_or_paused_therapy_callback) {
            active_or_paused_therapy_callback();
        }
    }
}

void add_and_send_new_other_state_info(NotificationType notification_type) {
    ESP_LOGI(TAG, "add_and_send_new_other_state_info");

    uint16_t passed_seconds = get_current_therapy_passed_duration();
    add_notification_log(notification_type, passed_seconds);
    restart_duration_update_watchdog_timer();

    TimerStateInfoMessage message;
    message.type = notification_type;
    message.duration = get_current_therapy_duration();
    message.therapy_id = get_current_therapy_id();
    message.therapy_passed_seconds = passed_seconds;

    if(notification_type == TIMER_STATE_PAUSED_THERAPY || notification_type == TIMER_STATE_INACTIVE) {
        message.remaining_seconds = get_inactivity_remaining_seconds();
    }
    else{
        message.remaining_seconds = get_alert_remaining_seconds();
    }

    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_timer_state_info_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }

        size_t len = strlen(json_str);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[TIMER_STATE_INFO_SIZE];
        size_t len = encode_timer_state_info_message_binary(&message, buf);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, buf, len);
    }
}

void add_and_send_new_therapy_state_info(NotificationType notification_type) {
    uint16_t therapy_id = get_current_therapy_id();
    uint16_t therapy_duration = get_current_therapy_duration();
    uint16_t passed_seconds = get_current_therapy_passed_duration();
    uint16_t remaining_seconds = get_therapy_remaining_seconds();

    uint8_t data[] = {therapy_id >> 8, therapy_id & 0xFF, therapy_duration >> 8, therapy_duration & 0xFF, passed_seconds >> 8, passed_seconds & 0xFF};
    add_log(notification_type, data, sizeof(data), passed_seconds);
    restart_duration_update_watchdog_timer();

    TimerStateInfoMessage message;
    message.type = notification_type;
    message.duration = therapy_duration;
    message.therapy_id = therapy_id;
    message.remaining_seconds = remaining_seconds;
    message.therapy_passed_seconds = passed_seconds;

    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_timer_state_info_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }

        size_t len = strlen(json_str);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[TIMER_STATE_INFO_SIZE];
        size_t len = encode_timer_state_info_message_binary(&message, buf);
        send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, buf, len);
    }
}

void register_active_or_paused_therapy_info(void (*callback)()) {
    active_or_paused_therapy_callback = callback;
}

void init_timer_state_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
}