#include "general_manager.h"

#include "state_manager.h"
#include "timer_management.h"
#include "current_therapy_info_manager.h"
#include "temperature_alarm_control.h"
#include "deep_sleep_manager.h"
#include "laser_driver_control.h"
#include "notification_info_message_creator.h"
#include "timer_state_info_message_creator.h"
#include "esp_log.h"

static const char *TAG = "GeneralManager";

static void on_state_changed(DeviceState new_state){
    if (new_state == STATE_TEMPERATURE_ALERT) {
        //set_laser_drivers_gpio_pin_status(false);
        set_laser_drivers_status(false);
    }
    else if (new_state == STATE_INACTIVE) {
        ESP_LOGI(TAG, "State inactive.");
        set_laser_drivers_status(false);
        if (get_helmet_state()) {
            //set_laser_drivers_status(true); //lazeri çalıştırmak demek değil. lazerin çalışabilir durumda olması.
        }
    }
    else if (new_state == STATE_ACTIVE) {
        ESP_LOGI(TAG, "State active.");
        set_laser_drivers_status(true);
    }
}

void start_device() {
    if(check_alert_status()) {
        enter_deep_sleep();
    }
    else{
        start_inactivity_timer();
    }
}

static void turn_off_device_because_of_inactivity() {
    if (!stop_inactivity_timer()) {
        return;
    }
    set_device_state(STATE_IDLE);
}

void set_inactivity_after_alert_expires() {
    if(is_alert_timer_running()) {
        stop_alert_timer();
        start_inactivity_timer();
    }
    else {
        //ERROR
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
            terminate_therapy();
            start_inactivity_timer();
        }
        else{
            ESP_LOGE(TAG, "Big error.");
            return;
        }
        add_and_send_notification_info(notification_type);
    }
}

void change_helmet_state(bool helmet_state) {
    if(helmet_state == get_helmet_state()) {
        return;
    }

    if(!set_helmet_state(helmet_state)){
        ESP_LOGE(TAG, "Helmet state can not be set.");
    }

    if (helmet_state) {
        ESP_LOGI(TAG, "Helmet is on.");
        add_and_send_notification_info(NOTIF_HELMET_ON);
    }
    else {
        add_and_send_notification_info(NOTIF_HELMET_OFF);
        if (get_device_state() == STATE_ACTIVE) {
            pause_therapy();
            start_inactivity_timer();
        }
    }
}

void throw_alert_for_temperature(uint8_t sensor_index) {
    if (get_device_state() != STATE_TEMPERATURE_ALERT){
        if(get_device_state() == STATE_ACTIVE) {
            pause_therapy_because_of_alert();
        }
        start_alert_timer(sensor_index);
    }
}

static void on_timer_start(NotificationType notification_type) {

    switch(notification_type) {
        case TIMER_STATE_INACTIVE:
            add_and_send_new_other_state_info(notification_type);
            break;
        case TIMER_STATE_NEW_THERAPY_BY_BUTTON:
        case TIMER_STATE_NEW_THERAPY_BY_APP:
        case TIMER_STATE_CONTINUE_THERAPY_BY_BUTTON:
        case TIMER_STATE_CONTINUE_THERAPY_BY_APP:
            add_and_send_new_therapy_state_info(notification_type);
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
    if(get_device_state() == STATE_ACTIVE) {
        if(is_therapy_timer_running()) {
            ESP_LOGI(TAG, "On activate when state active");
            stop_therapy_timer();
            set_device_state(STATE_IDLE);
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state active but therapy timer is not running.");
        }
    } else if(get_device_state() == STATE_INACTIVE && get_helmet_state()) {
        if(is_inactivity_timer_running()) {
            ESP_LOGI(TAG, "On activate when state inactive");
            stop_inactivity_timer();
            start_new_therapy(duration);
        }
        else{
            ESP_LOGE(TAG, "On activate when state inactive but inactivity timer is not running.");
        }
    }
}

void init_general_manager()
{
    init_state_manager();
    //init_timer_manager();
    register_state_change_callback(on_state_changed);
    register_timer_end_callback(on_timer_end);
    register_timer_state_change_callback(on_timer_start);
}