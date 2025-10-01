#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "transaction_manager.h"

#include "i2c_control.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"
#include "proximity_sensor_control.h"
#include "timer_management.h"
#include "binary_message_parser.h"
#include "state_manager.h"
#include "ble/include/ble_controller.h"
#include "transaction_message_encoder.h"
#include "message_encoder.h"
#include "storage/log_writer.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "ble/include/ble_connection_state_manager.h"
#include "message_queue_manager.h"
#include "notification_info_message_creator.h"
#include "records_info_message_creator.h"
#include "measurement_info_message_creator.h"
#include "timer_state_info_message_creator.h"
#include "device_info_message_creator.h"
#include "current_therapy_info_manager.h"
#include "general_manager.h"
#include "main_button_controller.h"


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
    uint16_t passed_seconds = get_session_passed_seconds();
    add_notification_log(BLE_DISCONNECTED, passed_seconds);
    restart_duration_update_watchdog_timer();
}

static void handle_status_change_message(const StatusChangeMessage *msg)
{
    if(msg->type == STOP) {
        ESP_LOGI(TAG, "STOP therapy");
        terminate_therapy();
        add_and_send_notification_info(NOTIF_THERAPY_STOPPED_BY_APP);
        start_inactivity_timer();
    } else if(msg->type == PAUSE) {
        ESP_LOGI(TAG, "PAUSE therapy");
        if (get_device_state() == STATE_ACTIVE) {
            pause_therapy();
        }
        else {
            ESP_LOGE(TAG, "Big error.");
        }
        add_and_send_notification_info(NOTIF_THERAPY_PAUSED_BY_APP);

    } else if(msg->type == CONTINUE) {
        ESP_LOGI(TAG, "CONTINUE therapy");
        if(get_device_state() == STATE_INACTIVE) {
            start_or_continue_therapy(true);
        }
        else {
            ESP_LOGE(TAG, "Big error.");
        }
    }
}

static void on_write_of_therapy_state(const uint8_t *buf, size_t len) {
    StatusChangeMessage status_change_message;
    if(!decode_status_change_message_bin(buf, &status_change_message)) {
        ESP_LOGE(TAG, "StatusChangeMessage decode failed");
        return;
    }

    ESP_LOGI(TAG, "Therapy status change received: type=%u",
         status_change_message.type);

    handle_status_change_message(&status_change_message);
}
/*
static void on_write_of_feedback_message(const char *data) {
    FeedbackMessage feedback_message;
    if(!decode_feedback_message(data, &feedback_message)) {
        return;
    }
    process_feedback_message(feedback_message.message_id);
}
*/


static void add_and_send_brightness_update(const uint8_t brightness[6]) {
    uint16_t passed_seconds = get_session_passed_seconds();

    // NOTIF_BRIGHTNESS_UPDATED: data_len tam 6 olmalı (log_utils.get_log_entry_size_info ile uyumlu)
    add_log(NOTIF_BRIGHTNESS_UPDATED, brightness, 6, passed_seconds);
    ESP_LOGE(TAG, "Log of brightness update with passed_seconds: %u", passed_seconds);

    // Bildirim Mesajı protokol gereği sadece type + passed_seconds içerir (parlaklık değerleri log'da tutuluyor)
    send_notification_info(NOTIF_BRIGHTNESS_UPDATED, passed_seconds);
}

static void handle_activation_message(const ActivationMessage *msg)
{
    if(msg->duration > 0) {
        try_start_new_therapy_by_activation(msg->duration);
    } 

    for(int i = 0; i < TOTAL_REGION_COUNT; i++) {
        set_brightness_of_region(i + 1, msg->brightness[i]);
    }
    add_and_send_brightness_update(msg->brightness);
}

static void on_write_of_activation_message(const uint8_t *buf, size_t len) {
    ActivationMessage activation_message;
    if(!decode_activation_message_bin(buf, &activation_message)) {
        return;
    }

    handle_activation_message(&activation_message);

    for(int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Laser Data received: %d", activation_message.brightness[i]);
    }
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

static void periodic_message_sender_task(void *pvParameters) {

    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "SIM: short-press (software)");
    while(1) {
        ESP_LOGI(TAG, "Do short press");
        do_short_press();
    
        vTaskDelay(2000);
    }
    /*
    const TickType_t delay = pdMS_TO_TICKS(5 * 1000); 
    const TickType_t gap_yield = pdMS_TO_TICKS(1500);
    uint8_t byte_data = 0xAB;
    vTaskDelay(delay * 3);
    request_conn_interval_ms(200);
    vTaskDelay(gap_yield);

    set_phy_2m();
    vTaskDelay(gap_yield);
    while(1) {

        ActivationMessage activationMsg;
        activationMsg.brightness[0] = 10;
        activationMsg.brightness[1] = 10;
        activationMsg.brightness[2] = 10;
        activationMsg.brightness[3] = 10;
        activationMsg.duration = 15;

        handle_activation_message(&activationMsg);
        
        vTaskDelay(2 * delay);

        ActivationMessage activationMsg2;
        activationMsg2.brightness[0] = 10;
        activationMsg2.brightness[1] = 20;
        activationMsg2.brightness[2] = 20;
        activationMsg2.brightness[3] = 0;
        activationMsg2.duration = 0;

        handle_activation_message(&activationMsg2);

        vTaskDelay(2 * delay);


        
        send_notification_info(NOTIF_HELMET_ON, 0);
        vTaskDelay(delay);

        send_info_message_to_queue( NOTIFICATION_INFO_MESSAGE, &byte_data, 1, 0);
        vTaskDelay(delay);
        
    }
        */
}

void init_transaction_manager(){
    init_ble();
    init_message_queue_manager();
    register_on_write_activation_callback(on_write_of_activation_message);
    //register_on_write_feedback_callback(on_write_of_feedback_message);
    register_on_write_updating_therapy_state_callback(on_write_of_therapy_state);
    register_on_disconnect_callback(on_disconnect_ble);
    init_message_creators();
    xTaskCreate(periodic_message_sender_task, "PeriodicMsgSender", 2048, NULL, 5, NULL);
}