#include <string.h>
#include "transaction_logic.h"
#include "state_manager.h"
#include "timer_management.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"
#include "transaction_manager.h"

void handle_activation_message(const ActivationMessage *msg)
{
    for(int i=0;i<6;i++) {
        set_brightness_of_region(i + 1, msg->brightness[i]);
    }

    if(msg->therapy_duration > 0) {
        if(get_device_state() == STATE_INACTIVITY || get_device_state() == STATE_ACTIVE) {
            if(get_helmet_state()) {
                start_new_therapy(msg->therapy_duration);
                add_and_send_notification_info(THERAPY_STARTED_BY_APP);
            }
        }
    } else if(get_device_state() == STATE_ACTIVE) {
        add_and_send_notification_info(REGIONS_BRIGHTNESS_UPDATED);
    }
}

void handle_status_change_message(const StatusChangeMessage *msg)
{
    if(strcmp(msg->type, "STOP") == 0) {
        if(get_device_state() == STATE_ACTIVE) {
            add_and_send_notification_info(THERAPY_STOPPED_BY_APP);
            stop_lasers();
            reset_passed_therapy_duration();
        }
    } else if(strcmp(msg->type, "PAUSE") == 0) {
        if(get_device_state() == STATE_ACTIVE) {
            add_and_send_notification_info(THERAPY_PAUSED_BY_APP);
            stop_lasers();
            update_passed_therapy_duration();
        }
    } else if(strcmp(msg->type, "CONTINUE") == 0) {
        if(get_device_state() == STATE_INACTIVITY) {
            start_therapy(true);
        }
    }
}

void handle_feedback_message(const FeedbackMessage *msg)
{
    if(msg->type == DEVICE_INFO_MESSAGE_ACK) {
        NotificationType n;
        if(get_device_state() == STATE_ACTIVE) n = CURRENT_STATE_THERAPY;
        else if(get_device_state() == STATE_INACTIVITY) n = CURRENT_STATE_INACTIVITY;
        else if(get_device_state() == STATE_TEMPERATURE_ALERT) n = CURRENT_STATE_TEMP_ALERT;
        else if(get_device_state() == STATE_HUMIDITY_ALERT) n = CURRENT_STATE_HUM_ALERT;
        else return;
        add_and_send_notification_info(n);
    } else if(msg->type == DEVICE_STATE_INFO_MESSAGE_ACK) {
        add_and_send_notification_info(get_helmet_state() ? HELMET_ON : HELMET_OFF);
    } else if(msg->type == HELMET_STATE_INFO_MESSAGE_ACK) {
        if(get_device_state() == STATE_ACTIVE) {
            add_and_send_notification_info(ONGOING_THERAPY);
        } else {
            uint8_t buf[3] = { get_temperature(), 0, 0 };
            send_info_message(MEASUREMENT_INFO_MESSAGE, buf, 3);
        }
    }
}
