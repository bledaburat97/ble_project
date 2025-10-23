#include "timer_state_info_message_creator.h"

#include "message_queue_manager.h"

#include "../storage/log_types.h"
#include "../storage/log_writer.h"

#include "../helper/binary_message_encoder.h"

#include "../state/current_therapy_info_manager.h"
#include "../state/timer_manager.h"
#include "../state/state_manager.h"

#include "esp_log.h"
#include <string.h>
#include "esp_err.h"
#include <stdlib.h>

static const char *TAG = "TimerStateInfoMessageCreator";
static void (*active_or_paused_therapy_callback)() = NULL;

static void log_timer_state_message(const TimerStateInfoMessage *message, const char *context) {
    ESP_LOGI(TAG,
             "(%s) timer state info with type: %u, duration: %u, therapy_id: %u, therapy_passed_seconds: %u, remaining_seconds: %u",
             context, message->type, message->duration, message->therapy_id,
             message->therapy_passed_seconds, message->remaining_seconds);
}

static void enqueue_timer_state_message(const TimerStateInfoMessage *message) {
    uint8_t buf[TIMER_STATE_INFO_SIZE];
    size_t len = encode_timer_state_info_message_binary(message, buf);
    send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, buf, len);
}

static void on_device_info_feedback_callback() {
    TimerStateInfoMessage message = {0};
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

    log_timer_state_message(&message, "Current state");
    enqueue_timer_state_message(&message);

    if(message.type == TIMER_STATE_ACTIVE_THERAPY || message.type == TIMER_STATE_PAUSED_THERAPY) {
        if(active_or_paused_therapy_callback) {
            active_or_paused_therapy_callback();
        }
    }
}

void add_and_send_new_other_state_info(NotificationType notification_type) {
    ESP_LOGI(TAG, "add_and_send_new_other_state_info");

    uint16_t therapy_passed_seconds = get_current_therapy_passed_duration();
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t notif_err = add_notification_log(notification_type, passed_seconds);
    if (notif_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist notification log for timer state: %s", esp_err_to_name(notif_err));
    }
    restart_duration_update_watchdog_timer();

    TimerStateInfoMessage message = {0};
    message.type = notification_type;
    message.duration = get_current_therapy_duration();
    message.therapy_id = get_current_therapy_id();
    message.therapy_passed_seconds = therapy_passed_seconds;

    if(notification_type == TIMER_STATE_PAUSED_THERAPY || notification_type == TIMER_STATE_INACTIVE) {
        message.remaining_seconds = get_inactivity_remaining_seconds();
    }
    else{
        message.remaining_seconds = get_alert_remaining_seconds();
    }

    log_timer_state_message(&message, "Other state");
    enqueue_timer_state_message(&message);
}

static void add_and_send_new_therapy_state_info(NotificationType notification_type, TimerStateInfoMessage message) {
    uint16_t passed_seconds = get_session_passed_seconds();

    uint8_t data[] = {message.therapy_id >> 8, message.therapy_id & 0xFF, message.duration >> 8, message.duration & 0xFF, message.therapy_passed_seconds >> 8, message.therapy_passed_seconds & 0xFF};

    esp_err_t err = add_log(notification_type, data, sizeof(data), passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist therapy state log (type=%u): %s", notification_type, esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG, "Log of therapy state with type: %u with passed_seconds: %u", notification_type, passed_seconds);
    }

    restart_duration_update_watchdog_timer();

    log_timer_state_message(&message, "New therapy state");
    enqueue_timer_state_message(&message);
}

void send_new_therapy_started(NotificationType notification_type) {
    uint16_t therapy_id = get_new_therapy_id_for_new_therapy();
    uint16_t duration = get_current_therapy_duration();
    uint16_t therapy_passed_seconds = 0;
    uint16_t remaining_seconds = get_therapy_remaining_seconds();

    TimerStateInfoMessage message = {
        .type = notification_type,
        .duration = duration,
        .therapy_id = therapy_id,
        .therapy_passed_seconds = therapy_passed_seconds,
        .remaining_seconds = remaining_seconds
    };

    add_and_send_new_therapy_state_info(notification_type, message);
}

void send_therapy_continued(NotificationType notification_type) {
    uint16_t therapy_id = get_current_therapy_id();
    uint16_t duration   = get_current_therapy_duration();
    uint16_t therapy_passed_seconds = get_passed_duration_before_last_pause();
    uint16_t remaining_seconds = (therapy_passed_seconds >= duration) ? 0 : (duration - therapy_passed_seconds);

    TimerStateInfoMessage message = {
        .type = notification_type,
        .duration = duration,
        .therapy_id = therapy_id,
        .therapy_passed_seconds = therapy_passed_seconds,
        .remaining_seconds = remaining_seconds
    };

    add_and_send_new_therapy_state_info(notification_type, message);
}

void register_active_or_paused_therapy_info(void (*callback)()) {
    active_or_paused_therapy_callback = callback;
}

void init_timer_state_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
}