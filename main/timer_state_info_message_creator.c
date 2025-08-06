#include "timer_state_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "timer_management.h"
#include "json_encoder.h"
#include "storage/log_writer.h"
#include "message_queue_manager.h"
#include "state_manager.h"
#include "therapy_message_counter.h"
#include <string.h>
#include "message_queue_manager.h"

static const char *TAG = "TimerStateInfoMessageCreator";
static void (*active_or_paused_therapy_callback)() = NULL;

static void on_device_info_feedback_callback() {
    TimerStateInfoMessage message;
    DeviceState device_state = get_device_state();
    if(device_state == STATE_ACTIVE) {
        message.type = TIMER_STATE_ACTIVE_THERAPY;
        message.duration = 0; //URGENT get remaining duration of therapy;
        message.therapy_id = 0; //URGENT
    }
    else if(device_state == STATE_INACTIVE) {
        //URGENT başlamış terapi varsa:
        message.type = TIMER_STATE_PAUSED_THERAPY;
        message.duration = 0; //URGENT get remaining duration of inactivity;
        message.therapy_id = 0; //URGENT

        //URGENT başlamış terapi yoksa:
        message.type = TIMER_STATE_INACTIVE;
        message.duration = 0; //URGENT get remaining duration of inactivity;
        message.therapy_id = 0;
    }
    else if(device_state == STATE_TEMPERATURE_ALERT) {
        message.type = TIMER_STATE_HIGH_TEMP_ALERT_1; //TODO: hangi sensörde hata varsa o olacak.
        message.duration = 0; //TODO get remaining duration of alert;
        message.therapy_id = 0;
    }

    else if(device_state == STATE_HUMIDITY_ALERT) {
        message.type = TIMER_STATE_LOW_HUM_ALERT; //TODO: hangi hata varsa o olacak.
        message.duration = 0; //TODO get remaining duration of alert;
        message.therapy_id = 0;
    }

    else {
        ESP_LOGE(TAG, "Device state: %u is not correct.", device_state);
        return;
    }

    message.passed_seconds = get_passed_duration();
    message.message_id = get_message_id();

    char *json_str = encode_timer_state_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    if(message.type == TIMER_STATE_ACTIVE_THERAPY || message.type == TIMER_STATE_PAUSED_THERAPY) {
        if(active_or_paused_therapy_callback) {
            active_or_paused_therapy_callback();
        }
    }
    free(json_str);
}


void add_and_send_new_other_state_info(NotificationType notification_type) {
    uint16_t passed_seconds = get_passed_duration();
    add_notification_log(notification_type, passed_seconds);

    TimerStateInfoMessage message;
    message.type = notification_type;
    message.duration = 0; //böyle kalabilir.
    message.therapy_id = 0; //URGENT
    message.passed_seconds = passed_seconds;
    message.message_id = get_message_id();

    char *json_str = encode_timer_state_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    free(json_str);
}

void add_and_send_new_therapy_state_info(NotificationType notification_type) {
    uint16_t therapy_id = 0; //URGENT
    uint16_t therapy_duration = 0; //URGENT
    uint16_t passed_seconds = get_passed_duration();
    uint8_t data[] = {therapy_id >> 8, therapy_id & 0xFF, therapy_duration >> 8, therapy_duration & 0xFF, passed_seconds >> 8, passed_seconds & 0xFF};
    add_log(notification_type, data, sizeof(data), passed_seconds);

    TimerStateInfoMessage message;
    message.type = notification_type;
    message.duration = therapy_duration;
    message.therapy_id = therapy_id;
    message.passed_seconds = passed_seconds;
    message.message_id = get_message_id();

    char *json_str = encode_timer_state_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    free(json_str);
}

void register_active_or_paused_therapy_info(void (*callback)()) {
    active_or_paused_therapy_callback = callback;
}

void init_timer_state_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
}