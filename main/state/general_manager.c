#include "general_manager.h"

#include "state_manager.h"
#include "timer_manager.h"
#include "current_therapy_info_manager.h"
#include "deep_sleep_manager.h"

#include "../transaction/notification_info_message_creator.h"
#include "../transaction/timer_state_info_message_creator.h"


#include "../i2c/temperature/temperature_alert_controller.h"
#include "../i2c/temperature/temperature_sensor_controller.h"
#include "../i2c/laser/laser_driver_controller.h"

#include "esp_log.h"

static const char *TAG = "GeneralManager";
static bool s_therapy_was_active_when_helmet_off = false;

static void on_state_changed(DeviceState new_state){
    if (new_state == STATE_TEMPERATURE_ALERT) {
        set_laser_drivers_status(false);
    }
    else if (new_state == STATE_INACTIVE) {
        set_laser_drivers_status(false);
    }
    else if (new_state == STATE_ACTIVE) {
        set_laser_drivers_status(true);
    }
}

void start_device() {
    if(check_alert_status()) {
        enter_deep_sleep();
    }
    else{
        if (!start_inactivity_timer()) {
            ESP_LOGE(TAG, "Failed to start inactivity timer during device start");
        }
    }
}

static void turn_off_device_because_of_inactivity() {
    if(is_inactivity_timer_running()) {
        stop_inactivity_timer();
    }
    set_device_state(STATE_IDLE);
}

static void set_inactivity_after_alert_expires() {
    if(is_alert_timer_running()) {
        stop_alert_timer();
    }
    if (!start_inactivity_timer()) {
        ESP_LOGE(TAG, "Failed to start inactivity timer during device start");
    }
}

static void on_timer_end(NotificationType notification_type) {
    if(notification_type == NOTIF_ALERT_TIMER_EXPIRED)
    {
        add_and_send_notification_info(notification_type);
        if(check_alert_status()) {
            enter_deep_sleep();
        }
        else{
            set_inactivity_after_alert_expires();
        }
    }
    else if(notification_type == NOTIF_INACTIVITY_TIMER_EXPIRED) {
        ESP_LOGI(TAG, "Inactivity timer expired!");
        turn_off_device_because_of_inactivity();
        add_and_send_notification_info(notification_type);
        enter_deep_sleep();
    }

    else if(notification_type == NOTIF_THERAPY_COMPLETED) {
        ESP_LOGI(TAG, "Therapy timer expired!");
        if(get_device_state() == STATE_ACTIVE) {
            uint16_t current_therapy_duration = get_current_therapy_duration();
            terminate_therapy();

            if(current_therapy_duration == INFINITE_THERAPY_DURATION) {
                start_or_continue_therapy_by_button();
                return;
            }

            add_and_send_notification_info(notification_type);
            if (!start_inactivity_timer()) {
                ESP_LOGE(TAG, "Failed to start inactivity timer during device start");
            }
        }
        else{
            ESP_LOGE(TAG, "Therapy should have been in active mode.");
            return;
        }
    }
}

void change_helmet_state(bool helmet_state) {
    bool old_helmet_state = get_helmet_state();

    if (helmet_state == old_helmet_state) {
        return;
    }

    if (!set_helmet_state(helmet_state)) {
        ESP_LOGE(TAG, "Helmet state can not be set.");
        return;
    }

    if (helmet_state) {
        ESP_LOGI(TAG, "Helmet is on.");

        if (is_helmet_off_debounce_timer_running()) {
            stop_helmet_off_debounce_timer();

            if (s_therapy_was_active_when_helmet_off &&
                get_current_therapy_state() == PAUSED) {
                ESP_LOGI(TAG, "Helmet back on during debounce window, continue therapy by app.");
                continue_therapy_by_app();
            }

            s_therapy_was_active_when_helmet_off = false;
        }

        add_and_send_notification_info(NOTIF_HELMET_ON);
    }
    else {
        ESP_LOGI(TAG, "Helmet state gets off");

        DeviceState device_state = get_device_state();
        s_therapy_was_active_when_helmet_off = (device_state == STATE_ACTIVE);
        if(s_therapy_was_active_when_helmet_off) {
            pause_therapy();
        }
        add_and_send_notification_info(NOTIF_HELMET_OFF);
        
        if (!start_helmet_off_debounce_timer()) {
            ESP_LOGE(TAG, "Failed to start helmet off debounce timer");
        }
    }
}

static void on_temp_alert(uint8_t sensor_index) {
    ESP_LOGE(TAG, "On temp alert");
    if (get_device_state() != STATE_TEMPERATURE_ALERT){
        ESP_LOGI(TAG, "State is not temperature alert.");
        if(get_device_state() == STATE_ACTIVE) {
            pause_therapy_because_of_alert();
        }
        ESP_LOGI(TAG, "Start alert timer.");
        start_alert_timer(sensor_index);
    }
}

static void on_timer_start(NotificationType notification_type) {
    switch(notification_type) {
        case TIMER_STATE_INACTIVE:
            if(get_current_therapy_state() == PAUSED){
                add_and_send_new_other_state_info(TIMER_STATE_PAUSED_THERAPY);
            }
            else{
                add_and_send_new_other_state_info(notification_type);
            }
            break;
        case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
        case TIMER_STATE_NEW_THERAPY_BY_APP:
            send_new_therapy_started(notification_type);
            break;
        case TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON:
        case TIMER_STATE_CONTINUE_THERAPY_BY_APP:
            send_therapy_continued(notification_type);
            break;
        case TIMER_STATE_LOW_TEMP_ALERT_1:
        case TIMER_STATE_HIGH_TEMP_ALERT_1:
        case TIMER_STATE_LOW_TEMP_ALERT_2:
        case TIMER_STATE_HIGH_TEMP_ALERT_2:
        case TIMER_STATE_LOW_TEMP_ALERT_3:
        case TIMER_STATE_HIGH_TEMP_ALERT_3:
            add_and_send_new_other_state_info(notification_type);
            break;
        default:
            ESP_LOGE(TAG, "This notification type: %u shouldn't have start a timer.", notification_type);
    }
}

void try_start_new_therapy_by_activation(uint16_t duration) {
    DeviceState deviceState = get_device_state();
    if(deviceState == STATE_ACTIVE) {
        if(is_therapy_timer_running()) {
            ESP_LOGI(TAG, "On activate when state active");
            terminate_therapy();
            set_device_state(STATE_IDLE);
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state active but therapy timer is not running.");
        }
    } else if(deviceState == STATE_INACTIVE && get_helmet_state()) {
        if(is_inactivity_timer_running()) {
            ESP_LOGI(TAG, "On activate when state inactive");
            stop_inactivity_timer();
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state inactive but inactivity timer is not running.");
        }
    }
    else {
        ESP_LOGE(TAG, "Therapy can not be started. device state: %u", deviceState);
    }
}

void init_general_manager()
{
    init_state_manager();
    register_state_change_callback(on_state_changed);
    register_timer_end_callback(on_timer_end);
    register_timer_state_change_callback(on_timer_start);
    register_temp_alert_callback(on_temp_alert);
}