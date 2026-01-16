#include "device_manager.h"

#include "state_controller.h"
#include "state_getter.h"
#include "therapy_duration_manager.h"
#include "../state/deep_sleep_manager.h"
#include "timer_controller.h"
#include "timer_info_getter.h"
#include "current_therapy_state_manager.h"
#include "session_timer_getter.h"
#include "session_timer_manager.h"
#include "helmet_off_debounce_timer_manager.h"
#include "message_saver.h"

#include "../temperature/temp_sensor_reader.h"
#include "../i2c/laser/laser_driver_controller.h"
#include "../i2c/proximity/proximity_sensor_controller.h"

#include "../transaction/notification_info_message_creator.h"
#include "../transaction/timer_state_info_message_creator.h"
#include "../transaction/default_configuration_handler.h"

#include "../storage/log_resume.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "DeviceManager";

static bool pause_active_therapy_if_running(const char *reason)
{
    if (get_current_therapy_state() != ACTIVE) {
        return false;
    }

    accumulate_paused_therapy_passed_duration(get_therapy_timer_passed_ms(), reason);
    stop_therapy_timer();
    set_current_therapy_state(PAUSED);
    return true;
}

static void set_alert_state(uint8_t sensor_index)
{
    if (get_device_state() == STATE_ACTIVE) {
        pause_active_therapy_if_running("Alert");
    } else {
        stop_inactivity_timer();
    }

    if (!start_alert_timer()) {
        ESP_LOGE(TAG, "Alert timer can not be activated.");
        enter_deep_sleep();
        return;
    }

    set_device_state(STATE_TEMPERATURE_ALERT);
    set_laser_drivers_status(false);

    switch (sensor_index) {
        case 0: add_and_send_new_other_state_info(TIMER_STATE_HIGH_TEMP_ALERT_1); break;
        case 1: add_and_send_new_other_state_info(TIMER_STATE_HIGH_TEMP_ALERT_2); break;
        case 2: add_and_send_new_other_state_info(TIMER_STATE_HIGH_TEMP_ALERT_3); break;
        default: break;
    }
}

static void start_inactivity(NotificationType notification_type)
{
    if (!start_inactivity_timer()) {
        ESP_LOGE(TAG, "Inactivity timer can not be activated.");
        enter_deep_sleep();
        return;
    }

    if (notification_type == TIMER_STATE_PAUSED_THERAPY) {
        set_current_therapy_state(PAUSED);
    } else if (notification_type == TIMER_STATE_INACTIVE) {
        clear_planned_therapy_duration();
        clear_paused_therapy_passed_duration();
        clear_session_clock();
        set_current_therapy_state(NONE);
    } else {
        enter_deep_sleep();
        return;
    }

    set_device_state(STATE_INACTIVE);
    set_laser_drivers_status(false);
    add_and_send_new_other_state_info(notification_type);
}

static void set_inactive_state(CurrentTherapyState therapy_state)
{
    if (therapy_state == PAUSED) {
        /*
        if (get_current_therapy_state() == PAUSED) {
            return;
        }
        */ //uncompleted therapy set etme işi bunu kaldırınca bozulmuş mudur, kontrol et.
        
        if (get_current_therapy_state() == ACTIVE) {
            pause_active_therapy_if_running("Manual");
        }

        start_inactivity(TIMER_STATE_PAUSED_THERAPY);
        return;
    }

    if (therapy_state == NONE) {
        start_inactivity(TIMER_STATE_INACTIVE);
        return;
    }

    ESP_LOGE(TAG, "Wrong therapy state is requested.");
    start_inactivity(TIMER_STATE_INACTIVE);
}

static void set_active_state(uint16_t duration, NotificationType start_reason, bool is_new, bool is_restart)
{
    DeviceState old_device_state = get_device_state();

    if (old_device_state == STATE_ACTIVE && is_new) {

        if (!is_restart) {
            stop_therapy_timer();
        }

        clear_planned_therapy_duration();
        clear_paused_therapy_passed_duration();

        if (!try_set_planned_therapy_duration(duration)) {
            ESP_LOGE(TAG, "New therapy can not be set.");
            start_inactivity(TIMER_STATE_INACTIVE);
            return;
        }

        clear_paused_therapy_passed_duration();
        reset_session_clock(0);

        if (!start_therapy_timer(duration)) {
            ESP_LOGE(TAG, "Therapy timer can not be activated.");
            start_inactivity(TIMER_STATE_INACTIVE);
            return;
        }

        set_device_state(STATE_ACTIVE);
        set_laser_drivers_status(true);
        send_new_therapy_started(start_reason);
        set_current_therapy_state(ACTIVE);
        return;
    }

    if (old_device_state == STATE_INACTIVE && get_helmet_state()) {
        CurrentTherapyState old_current_therapy_state = get_current_therapy_state();
        stop_inactivity_timer();

        if (is_new) {
            clear_planned_therapy_duration();
            clear_paused_therapy_passed_duration();
            reset_session_clock(0);

            if (!try_set_planned_therapy_duration(duration)) {
                ESP_LOGE(TAG, "New therapy can not be set.");
                start_inactivity(TIMER_STATE_INACTIVE);
                return;
            }
        } else if (old_current_therapy_state == PAUSED) {
            if (!check_uncompleted_paused_therapy_exists()) {
                ESP_LOGE(TAG, "There should be paused therapy");
                start_inactivity(TIMER_STATE_INACTIVE);
                return;
            }
        } else {
            ESP_LOGE(TAG, "Wrong request.");
            start_inactivity(TIMER_STATE_INACTIVE);
            return;
        }

        if (!start_therapy_timer(duration)) {
            ESP_LOGE(TAG, "Therapy timer can not be activated.");
            start_inactivity(TIMER_STATE_INACTIVE);
            return;
        }

        set_device_state(STATE_ACTIVE);
        set_laser_drivers_status(true);

        if (is_new) {
            send_new_therapy_started(start_reason);
        } else {
            send_therapy_continued(start_reason);
        }

        set_current_therapy_state(ACTIVE);
        return;
    }

    ESP_LOGE(TAG, "Wrong request.");
    start_inactivity(TIMER_STATE_INACTIVE);
}

void handle_device_event(const DeviceEvent *event)
{
    switch (event->type) {

        case EVT_HELMET_ON: {
            ESP_LOGI(TAG, "EVT_HELMET_ON");
            add_and_send_notification_info(NOTIF_HELMET_ON);

            if (is_helmet_off_debounce_timer_running()) {
                stop_helmet_off_debounce_timer();

                if (get_current_therapy_state() == PAUSED) {
                    uint16_t remaining_therapy_duration = get_remaining_therapy_duration();
                    set_active_state(remaining_therapy_duration, TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON, false, false);
                } else {
                    ESP_LOGE(TAG, "Helmet debounce stopped but therapy state is not PAUSED");
                }
            }
        } break;

        case EVT_HELMET_OFF: {
            ESP_LOGI(TAG, "EVT_HELMET_OFF");

            if (get_device_state() == STATE_ACTIVE) {
                set_inactive_state(PAUSED);
                if (!start_helmet_off_debounce_timer()) {
                    ESP_LOGE(TAG, "Failed to start helmet off debounce timer");
                }
            }

            add_and_send_notification_info(NOTIF_HELMET_OFF);
        } break;

        case EVT_HELMET_OFF_DEBOUNCE_COMPLETED: {
            ESP_LOGI(TAG, "EVT_HELMET_OFF_DEBOUNCE_COMPLETED");
        } break;

        case EVT_TEMPERATURE_ALERT: {
            ESP_LOGW(TAG, "EVT_TEMPERATURE_ALERT sensor=%u", (unsigned)event->data.temp_alert.sensor_index);
            set_alert_state(event->data.temp_alert.sensor_index);
        } break;

        case EVT_ALERT_TIMER_COMPLETED: {
            ESP_LOGI(TAG, "EVT_ALERT_TIMER_COMPLETED");
            add_and_send_notification_info(NOTIF_ALERT_TIMER_EXPIRED);

            if (temp_sensor_reader_is_any_alerted_sensor()) {
                enter_deep_sleep();
            } else {
                set_inactive_state(get_current_therapy_state());
            }
        } break;

        case EVT_INACTIVITY_TIMER_COMPLETED: {
            ESP_LOGI(TAG, "EVT_INACTIVITY_TIMER_COMPLETED");
            add_and_send_notification_info(NOTIF_INACTIVITY_TIMER_EXPIRED);
            enter_deep_sleep();
        } break;

        case EVT_THERAPY_TIMER_COMPLETED: {
            ESP_LOGI(TAG, "EVT_THERAPY_TIMER_COMPLETED");

            if (get_device_state() != STATE_ACTIVE) {
                ESP_LOGE(TAG, "Therapy completed but state not ACTIVE");
                break;
            }

            uint16_t duration = get_planned_therapy_duration_s();

            clear_planned_therapy_duration();
            clear_paused_therapy_passed_duration();
            clear_session_clock();

            if (duration == INFINITE_THERAPY_DURATION) {
                set_active_state(duration, TIMER_STATE_NEW_THERAPY_BY_BUTTON, true, true);
                break;
            }

            add_and_send_notification_info(NOTIF_THERAPY_COMPLETED);
            set_inactive_state(NONE);
        } break;

        case EVT_ACTIVATION_REQUEST: {
            const ActivationPayload *payload = &event->data.activation;
            ESP_LOGI(TAG, "EVT_ACTIVATION_REQUEST duration=%u", (unsigned)payload->duration_s);
            if(payload->brightness_present){
                uint16_t passed_seconds = get_session_passed_seconds();
                change_brightness(payload->brightness);
                save_log(NOTIF_BRIGHTNESS_UPDATED, payload->brightness, 6, passed_seconds);
                send_notification_info(NOTIF_BRIGHTNESS_UPDATED, passed_seconds);
            }
            if (payload->duration_s > 0) {
                set_active_state(payload->duration_s, TIMER_STATE_NEW_THERAPY_BY_APP, true, false);
            }
        } break;

        case EVT_PAUSE_REQUEST: {
            ESP_LOGI(TAG, "EVT_PAUSE_REQUEST");
            if (get_device_state() == STATE_ACTIVE) {
                set_inactive_state(PAUSED);
                add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_APP);
            }
        } break;

        case EVT_CONTINUE_REQUEST: {
            ESP_LOGI(TAG, "EVT_CONTINUE_REQUEST");
            uint16_t remaining_therapy_duration = get_remaining_therapy_duration();
            set_active_state(remaining_therapy_duration, TIMER_STATE_CONTINUE_THERAPY_BY_APP, false, false);
        } break;

        case EVT_STOP_REQUEST: {
            ESP_LOGI(TAG, "EVT_STOP_REQUEST");
            set_inactive_state(NONE);
            add_and_send_notification_info(NOTIF_THERAPY_STOPPED_BY_APP);
        } break;

        case EVT_SHORT_BUTTON_PRESS: {
            ESP_LOGI(TAG, "EVT_SHORT_BUTTON_PRESS");

            DeviceState device_state = get_device_state();

            if (device_state == STATE_INACTIVE) {
                if (get_current_therapy_state() == PAUSED) {
                    uint16_t remaining_therapy_duration = get_remaining_therapy_duration();
                    set_active_state(remaining_therapy_duration, TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON, false, false);
                } else if (get_current_therapy_state() == NONE) {
                    uint16_t default_therapy_duration = get_default_therapy_duration();
                    set_active_state(default_therapy_duration, TIMER_STATE_NEW_THERAPY_BY_BUTTON, true, false);
                }
            } else if (device_state == STATE_ACTIVE) {
                set_inactive_state(PAUSED);
                add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_BUTTON);
            } else {
                ESP_LOGW(TAG, "Short press ignored: unexpected device state (%d).", device_state);
            }
        } break;

        case EVT_LONG_BUTTON_PRESS: {
            ESP_LOGI(TAG, "EVT_LONG_BUTTON_PRESS");
            uint16_t passed_seconds = get_session_passed_seconds();
            save_log(NOTIF_SHUT_DOWN_BY_BUTTON, NULL, 0, passed_seconds);
            enter_deep_sleep();
        } break;

        case EVT_UNCOMPLETED_THERAPY_SET: {
            const UncompletedTherapyInfo *payload = &event->data.uncompleted;

            if (try_set_planned_therapy_duration(payload->therapy_duration) && payload->therapy_passed_seconds > 0) {
                uint16_t passed_seconds = get_session_passed_seconds();
                change_brightness(payload->last_brightness);
                save_log(NOTIF_BRIGHTNESS_UPDATED, payload->last_brightness, 6, passed_seconds);
                send_notification_info(NOTIF_BRIGHTNESS_UPDATED, passed_seconds);
                set_paused_therapy_passed_duration_ms(payload->therapy_passed_seconds);
                reset_session_clock(payload->last_passed_seconds * 1000000u);
                set_continue_uncompleted_therapy(true);
                set_inactive_state(PAUSED);
            } else {
                clear_planned_therapy_duration();
                clear_paused_therapy_passed_duration();
                set_inactive_state(NONE);
            }

        } break;

        case EVT_DEVICE_START : {
            clear_planned_therapy_duration();
            clear_paused_therapy_passed_duration();
            set_inactive_state(NONE);
        } break;

        default:
            break;
    }
}
