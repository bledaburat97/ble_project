#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "i2c_control.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"
#include "proximity_sensor_control.h"
#include "timer_management.h"
#include "json_parser.h"
#include "state_manager.h"
#include "ble_control.h"
#include "transaction_manager.h"
#include "transaction_message_encoder.h"
#include "json_encoder.h"
#include "log_writer.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "ble/ble_state_manager.h"
#include "message_queue_manager.h"
#include "notification_info_message_creator.h"
#include "records_info_message_creator.h"
#include "measurement_info_message_creator.h"
#include "timer_state_info_message_creator.h"
#include "device_info_message_creator.h"

#ifndef UNIT_TESTING
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#else
#include "fake_freertos.h"
#include "fake_task.h"
#include "fake_esp_log.h"
#include "fake_esp_err.h"
#include "fake_esp_mac.h"
#include "fake_semphr.h"
#endif

static const char *TAG = "TransactionManager";

static void on_disconnect_ble() {
    set_ble_connection_status(false);
    uint16_t passed_seconds = get_passed_duration();
    add_notification_log(BLE_DISCONNECTED, passed_seconds);
}

static void on_write_of_therapy_state(const char *data) {
    StatusChangeMessage status_change_message;
    if(!decode_status_change_message(data, &status_change_message)) {
        ESP_LOGE(TAG, "StatusChangeMessage decode failed");
        return;
    }

    ESP_LOGI(TAG, "Therapy status change received: type=%s, therapy_id=%u",
         status_change_message.type, status_change_message.therapy_id);

    handle_status_change_message(&status_change_message);
}

void on_write_of_record_request_message(const char *data) {
    ESP_LOGI(TAG, "on_write_of_record_request_message");

    UpdateRecordRequestMessage record_request_message;
    if(!decode_update_record_request_message(data, &record_request_message)) {
        return;
    }

    uint16_t last_therapy_id_saved_in_app = record_request_message.last_therapy_id;

    uint16_t last_saved_therapy_id = read_therapy_count() - 1;
    ESP_LOGI(TAG, "last saved therapy_id: %d", last_saved_therapy_id);

    //active therapy'nin bilgilerini zaten aktif terapi bilgi mesajında göndermiş olmalıyız.
    for(int therapy_id = get_device_state() == STATE_ACTIVE ? (last_saved_therapy_id - 1) : last_saved_therapy_id; therapy_id > last_therapy_id_saved_in_app; therapy_id--) {
        send_records_info_message(therapy_id);
    }
}

static void on_write_of_feedback_message(const char *data) {
    FeedbackMessage feedback_message;
    if(!decode_feedback_message(data, &feedback_message)) {
        return;
    }
    handle_feedback_message(&feedback_message);
}

static void on_write_of_activation_message(const char *data) {
    ActivationMessage activation_message;
    if(!decode_activation_message(data, &activation_message)) {
        return;
    }

    handle_activation_message(&activation_message);

    for(int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Laser Data received: %d", activation_message.brightness[i]);
    }
}

void handle_activation_message(const ActivationMessage *msg)
{
    for(int i=0;i<6;i++) {
        set_brightness_of_region(i + 1, msg->brightness[i]);
    }

    if(msg->duration > 0) {
        if(get_device_state() == STATE_INACTIVE || get_device_state() == STATE_ACTIVE) {
            if(get_helmet_state()) {
                start_new_therapy(msg->duration);
            }
        }
    } else if(get_device_state() == STATE_ACTIVE) {
        add_and_send_notification_info(NOTIF_BRIGHTNESS_UPDATED);
    }
}

void handle_status_change_message(const StatusChangeMessage *msg)
{
    if(strcmp(msg->type, "STOP") == 0) {
        if(get_device_state() == STATE_ACTIVE) {
            add_and_send_notification_info(NOTIF_THERAPY_STOPPED_BY_APP);
            stop_lasers();
            reset_passed_therapy_duration();
        }
    } else if(strcmp(msg->type, "PAUSE") == 0) {
        if(get_device_state() == STATE_ACTIVE) {
            add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_APP);
            stop_lasers();
            update_passed_therapy_duration();
        }
    } else if(strcmp(msg->type, "CONTINUE") == 0) {
        if(get_device_state() == STATE_INACTIVE) {
            start_therapy(true);
        }
    }
}

void handle_feedback_message(const FeedbackMessage *msg)
{
    process_feedback_message(msg->message_id);
}

static void init_ble(){
    init_ble_state_manager();
    ESP_ERROR_CHECK(init_bluetooth());
    start_registering_and_advertising();
}

static void init_message_creators(){
    init_notification_info_message_creator();
    init_device_info_message_creator();
    init_measurement_info_message_creator();
    init_timer_state_info_message_creator();
    init_records_info_message_creator();
}

void init_transaction_manager(){
    init_ble();
    init_message_queue_manager();
    register_on_write_activation_callback(on_write_of_activation_message);
    register_on_write_feedback_callback(on_write_of_feedback_message);
    register_on_write_updating_records_callback(on_write_of_record_request_message);
    register_on_write_updating_therapy_state_callback(on_write_of_therapy_state);
    register_on_disconnect_callback(on_disconnect_ble);
    init_message_creators();
}