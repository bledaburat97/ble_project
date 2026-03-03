#include "timer_state_info_message_creator.h"

#include "message_queue_manager.h"

#include "../storage/log_types.h"

#include "../helper/binary_message_encoder.h"

#include "../manager/session_timer_getter.h"
#include "../manager/state_getter.h"
#include "../manager/current_therapy_state_manager.h"
#include "../manager/therapy_duration_manager.h"
#include "../manager/therapy_id_manager.h"
#include "../manager/timer_info_getter.h"
#include "../manager/message_saver.h"
#include "../manager/profile_manager.h"

#include "../device_configuration.h"

#include "esp_log.h"
#include <string.h>
#include "esp_err.h"
#include <stdlib.h>

static const char *TAG = "TimerStateInfoMessageCreator";
static void (*active_or_paused_therapy_callback)() = NULL;

// Timer state mesajını loglar (debug için).
static void log_timer_state_message(const TimerStateInfoMessage *message, const char *context) {
    ESP_LOGI(TAG,
             "(%s) timer state info with type: %u, duration: %u, therapy_id: %u, therapy_passed_seconds: %u, remaining_seconds: %u, session passed seconds: %u",
             context, message->type, message->duration, message->therapy_id,
             message->therapy_passed_seconds, message->remaining_seconds, message->passed_seconds);
}

// Timer state mesajını encode edip kuyruğa ekler.
static void enqueue_timer_state_message(const TimerStateInfoMessage *message) {
    uint8_t buf[TIMER_STATE_INFO_SIZE];
    size_t len = encode_timer_state_info_message_binary(message, buf);
    send_info_message_to_queue(TIMER_STATE_INFO_MESSAGE, buf, len);
}

// İlk senkron sonrası mevcut state snapshot'ını gönderir.
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
        message.duration = get_planned_therapy_duration_s();
        message.therapy_id = get_current_therapy_id();
        message.remaining_seconds = get_therapy_timer_remaining_s();
        message.therapy_passed_seconds = (uint16_t)((get_paused_therapy_passed_duration_ms() + get_therapy_timer_passed_ms()) /1000u);
    }
    else if(device_state == STATE_INACTIVE) {
        if(currentTherapyState == PAUSED) {
            message.type = TIMER_STATE_PAUSED_THERAPY;
            message.therapy_id = get_current_therapy_id();
            message.duration = get_planned_therapy_duration_s();
            message.remaining_seconds = get_inactivity_timer_remaining_s();
            message.therapy_passed_seconds = (uint16_t)(get_paused_therapy_passed_duration_ms() / 1000u);
        } 
        else if(currentTherapyState == NONE) {
            message.type = TIMER_STATE_INACTIVE;
            message.therapy_id = 0;
            message.duration = INACTIVITY_THRESHOLD_SECONDS;
            message.remaining_seconds = get_inactivity_timer_remaining_s();
            message.therapy_passed_seconds = 0;
        }
        else {
            ESP_LOGE(TAG, "Current therapy state is not set correctly");
            return;
        }        
    }
    else if(device_state == STATE_TEMPERATURE_ALERT) {
        if (currentTherapyState == PAUSED) {
            message.therapy_id = get_current_therapy_id();
            message.therapy_passed_seconds = (uint16_t)(get_paused_therapy_passed_duration_ms() / 1000u);
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
        message.duration = ALERT_THRESHOLD_SECONDS;
        message.remaining_seconds = get_alert_timer_remaining_s();
    }
    else {
        ESP_LOGE(TAG, "Device state: %u is not correct.", device_state);
        return;
    }

    message.passed_seconds = get_session_passed_seconds();
    ESP_LOGW(TAG, "Timer state message is created.");
    log_timer_state_message(&message, "Current state");
    enqueue_timer_state_message(&message);

    if(message.type == TIMER_STATE_ACTIVE_THERAPY || message.type == TIMER_STATE_PAUSED_THERAPY) {
        if(active_or_paused_therapy_callback) {
            active_or_paused_therapy_callback();
        }
    }
}

// Pause/Inactive/Alert gibi diğer state bildirimlerini üretir.
void add_and_send_new_other_state_info(NotificationType notification_type) {
    ESP_LOGI(TAG, "Add and send new other state info.");

    uint16_t therapy_passed_seconds = (uint16_t)(get_paused_therapy_passed_duration_ms() / 1000u);
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t notif_err = save_log(notification_type, NULL, 0, passed_seconds);
    if (notif_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist notification log for timer state: %s", esp_err_to_name(notif_err));
    }
    restart_duration_update_watchdog_timer();

    TimerStateInfoMessage message = {0};
    message.type = notification_type;
    message.duration = get_planned_therapy_duration_s();
    message.therapy_id = get_current_therapy_id();
    message.therapy_passed_seconds = therapy_passed_seconds;
    message.passed_seconds = passed_seconds;

    if(notification_type == TIMER_STATE_PAUSED_THERAPY || notification_type == TIMER_STATE_INACTIVE) {
        message.remaining_seconds = get_inactivity_timer_remaining_s();
    }
    else{
        message.remaining_seconds = get_alert_timer_remaining_s();
    }

    log_timer_state_message(&message, "Other state");
    enqueue_timer_state_message(&message);
}

// Yeni/continue terapi state mesajını loglayıp gönderir.
static void add_and_send_new_therapy_state_info(NotificationType notification_type, TimerStateInfoMessage message) {
    uint8_t data[] = {message.therapy_id >> 8, message.therapy_id & 0xFF, message.duration >> 8, message.duration & 0xFF, message.therapy_passed_seconds >> 8, message.therapy_passed_seconds & 0xFF};

    esp_err_t err = save_log(notification_type, data, sizeof(data), message.passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist therapy state log (type=%u): %s", notification_type, esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG, "Log of therapy state with type: %u with passed_seconds: %u", notification_type, message.passed_seconds);
    }

    restart_duration_update_watchdog_timer();

    log_timer_state_message(&message, "New therapy state");
    enqueue_timer_state_message(&message);
}

// Yeni terapi başlangıcını bildirir.
void send_new_therapy_started(NotificationType notification_type) {
    uint16_t therapy_id = get_new_therapy_id_for_new_therapy();
    uint16_t duration = get_planned_therapy_duration_s();
    uint16_t therapy_passed_seconds = 0;
    uint16_t remaining_seconds = get_therapy_timer_remaining_s();

    TimerStateInfoMessage message = {
        .type = notification_type,
        .duration = duration,
        .therapy_id = therapy_id,
        .therapy_passed_seconds = therapy_passed_seconds,
        .remaining_seconds = remaining_seconds,
        .passed_seconds = get_session_passed_seconds()
    };

    try_append_profile(therapy_id);
    add_and_send_new_therapy_state_info(notification_type, message);
}

// Pause edilmiş terapiyi devam ettirirken bildirir.
void send_therapy_continued(NotificationType notification_type) {
    uint16_t therapy_id = get_current_therapy_id();
    uint16_t duration = get_planned_therapy_duration_s();
    uint16_t therapy_passed_seconds = (uint16_t)((get_paused_therapy_passed_duration_ms() + get_therapy_timer_passed_ms()) /1000u);
    uint16_t remaining_seconds = (therapy_passed_seconds >= duration) ? 0 : (duration - therapy_passed_seconds);

    TimerStateInfoMessage message = {
        .type = notification_type,
        .duration = duration,
        .therapy_id = therapy_id,
        .therapy_passed_seconds = therapy_passed_seconds,
        .remaining_seconds = remaining_seconds,
        .passed_seconds = get_session_passed_seconds()
    };

    add_and_send_new_therapy_state_info(notification_type, message);
}

// Aktif/paused state gönderiminden sonra çağrılacak callback'i bağlar.
void register_active_or_paused_therapy_info(void (*callback)()) {
    active_or_paused_therapy_callback = callback;
}

// Timer state akışını başlatır (callback'leri bağlar).
void init_timer_state_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
}
