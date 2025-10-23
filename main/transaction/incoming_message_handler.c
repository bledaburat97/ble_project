#include "incoming_message_handler.h"

#include "notification_info_message_creator.h"

#include "../state/current_therapy_info_manager.h"
#include "../state/timer_manager.h"
#include "../state/state_manager.h"
#include "../state/general_manager.h"

#include "../i2c/laser/laser_driver_controller.h"

#include "../helper/binary_message_parser.h"

#include "../storage/log_writer.h"
#include "../storage/log_types.h"

#include "../ble/include/ble_controller.h"

#include "../button/main_button_controller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>


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

static const char *TAG = "IncomingMessageHandler";

static void handle_status_change_message(const StatusChangeMessage *msg)
{
    if(msg->type == STOP) {
        ESP_LOGI(TAG, "STOP therapy");
        terminate_therapy();
        add_and_send_notification_info(NOTIF_THERAPY_STOPPED_BY_APP);
        if (!start_inactivity_timer()) {
            ESP_LOGE(TAG, "Failed to start inactivity timer after STOP command");
        }
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

static void add_and_send_brightness_update(const uint8_t brightness[6]) {
    uint16_t passed_seconds = get_session_passed_seconds();

    // NOTIF_BRIGHTNESS_UPDATED: data_len tam 6 olmalı (log_utils.get_log_entry_size_info ile uyumlu)
    esp_err_t err = add_log(NOTIF_BRIGHTNESS_UPDATED, brightness, 6, passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist brightness update log: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Log of brightness update with passed_seconds: %u", passed_seconds);
    }

    // Bildirim Mesajı protokol gereği sadece type + passed_seconds içerir (parlaklık değerleri log'da tutuluyor)
    send_notification_info(NOTIF_BRIGHTNESS_UPDATED, passed_seconds);
}

static void handle_activation_message(const ActivationMessage *msg)
{
    if(msg->duration > 0) {
        try_start_new_therapy_by_activation(msg->duration);
    }
    else {
        ESP_LOGW(TAG, "Msg duration is zero: %u", msg->duration);
    }

    for(int i = 0; i < TOTAL_REGION_COUNT; i++) {
        set_brightness_of_region((uint8_t)(i + 1), msg->brightness[i]);
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
    ESP_LOGI(TAG, "Activation duration: %u", activation_message.duration);
}

static void periodic_message_sender_task(void *pvParameters) {

    vTaskDelay(pdMS_TO_TICKS(60000));
    ESP_LOGI(TAG, "SIM: short-press (software)");
    
    while(1) {
        ESP_LOGI(TAG, "Do short press");
        do_short_press();
    
        vTaskDelay(pdMS_TO_TICKS(20000));
    }
    
/*
    while(1) {

        //terapi başlat.
        ActivationMessage activationMsg;
        activationMsg.brightness[0] = 10;
        activationMsg.brightness[1] = 10;
        activationMsg.brightness[2] = 10;
        activationMsg.brightness[3] = 10;
        activationMsg.duration = 60;
        handle_activation_message(&activationMsg);

        vTaskDelay(pdMS_TO_TICKS(10000));

        //parlaklık değiştir.
        ActivationMessage activationMsg2;
        activationMsg2.brightness[0] = 10;
        activationMsg2.brightness[1] = 20;
        activationMsg2.brightness[2] = 20;
        activationMsg2.brightness[3] = 0;
        activationMsg2.duration = 0;
        handle_activation_message(&activationMsg2);

        vTaskDelay(pdMS_TO_TICKS(20000));

        //StatusChangeMessage status_change_message;
        //status_change_message.type = PAUSE;
        //handle_status_change_message(&status_change_message);
        
        //vTaskDelay(pdMS_TO_TICKS(10000));

        StatusChangeMessage status_change_message2;
        status_change_message2.type = CONTINUE;
        handle_status_change_message(&status_change_message2);

        vTaskDelay(pdMS_TO_TICKS(10000));

        StatusChangeMessage status_change_message3;
        status_change_message3.type = STOP;
        handle_status_change_message(&status_change_message3);

        vTaskDelay(pdMS_TO_TICKS(22000));

    }
*/
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

void init_incoming_message_handler(){
    register_on_write_activation_callback(on_write_of_activation_message);
    register_on_write_updating_therapy_state_callback(on_write_of_therapy_state);
    /*
    if (xTaskCreate(periodic_message_sender_task, "PeriodicMsgSender", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create periodic message sender task");
    }
    */
}