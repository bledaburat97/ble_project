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
#include "activation_command_manager.h"
#include "transaction_message_encoder.h"
#include "json_encoder.h"
#include "log_writer.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "ble/ble_state_manager.h"

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

static void send_info_message(MessageType message_type, uint8_t* data, size_t data_length)
{
    if (!get_ble_connection_status()) {
        ESP_LOGW(TAG, "No active BLE connection, cannot send message.");
        return;
    }

    int retry_count = 0;
    const int max_retries = 2;  // Set the maximum number of retries
    const int retry_delay_ms = 100;  // Delay between retries in milliseconds
    SemaphoreHandle_t ble_mutex = get_ble_mutex_handle();

    while (retry_count < max_retries) {
        if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            esp_err_t ret = ble_send_info_message_with_type(message_type, data, data_length);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Successfully sent message.");
            } else {
                ESP_LOGE(TAG, "Failed to send message: %s", esp_err_to_name(ret));
            }

            xSemaphoreGive(ble_mutex);
            return;
        } else {
            retry_count++;
            ESP_LOGW(TAG, "Failed to acquire BLE mutex, retrying... (%d/%d)", retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));  // Wait before retrying
        }
    }

    // If the retries are exhausted, log the failure
    ESP_LOGE(TAG, "Failed to acquire BLE mutex after %d attempts. Data not sent.", max_retries);
}

static void send_notification_info(NotificationType type) {
    NotificationMessage message;
    message.type = type;
    message.passed_seconds = get_passed_duration();
    
    char *json_str = encode_notification_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message(NOTIFICATION_INFO_MESSAGE, (uint8_t*)json_str, len);

    free(json_str);  // cJSON_PrintUnformatted ile heap'e alındığı için temizlenmeli
}

void add_and_send_notification_info(NotificationType notification_type) {
    uint16_t passed_seconds = get_passed_duration();
    add_notification_log(notification_type, passed_seconds);
    send_notification_info(notification_type);
}

static void add_and_send_active_therapy_info(NotificationType notification_type) {
    uint16_t therapy_id = 0; //URGENT
    uint16_t therapy_duration = 0; //URGENT
    uint16_t passed_seconds = get_passed_duration();
    uint8_t data[] = {therapy_id >> 8, therapy_id & 0xFF, therapy_duration >> 8, therapy_duration & 0xFF, passed_seconds >> 8, passed_seconds & 0xFF};
    add_log(notification_type, data, sizeof(data), passed_seconds);


    TherapyActivationInfoMessage message;
    message.type = notification_type;
    message.therapy_dur = therapy_duration;
    message.therapy_id = therapy_id;
    message.passed_seconds = passed_seconds;
    
    char *json_str = encode_therapy_activation_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message(ACTIVE_THERAPY_INFO_MESSAGE, (uint8_t*)json_str, len);

    free(json_str);
}

static void add_and_send_measurement_info(uint8_t temperature) {
    uint8_t humidity = 0; //URGENT
    uint8_t data[] = {temperature, humidity}; //URGENT eğer temp veya hum değişmişse.
    uint16_t passed_seconds = get_passed_duration();
    add_log(MEASUREMENT_CHANGED, data, sizeof(data), passed_seconds);


    MeasurementInfoMessage message;
    message.temperature = temperature;
    message.humidity = humidity;
    message.passed_seconds = passed_seconds;
    
    char *json_str = encode_measurement_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message(MEASUREMENT_INFO_MESSAGE, (uint8_t*)json_str, len);

    free(json_str);
}

static void add_and_send_device_info() {
    uint16_t passed_seconds = get_passed_duration();
    add_notification_log(BLE_CONNECTED, passed_seconds);

    DeviceInfoMessage message;
    //message.current_time; //TODO: set current time when RTC integrated.
    esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    message.passed_seconds = passed_seconds;
    message.last_saved_therapy_id = read_therapy_count() - 1;
    char *json_str = encode_device_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }
    size_t len = strlen(json_str);
    send_info_message(DEVICE_INFO_MESSAGE, (uint8_t*)json_str, len);

    free(json_str);
}

static void on_timer_end(NotificationType notification_type) {
    add_and_send_notification_info(notification_type);
}

static void on_timer_start(NotificationType notification_type) {
    if(notification_type == THERAPY_STARTED_BY_BUTTON || notification_type == THERAPY_STARTED_BY_APP
         || notification_type == THERAPY_CONTINUED_BY_BUTTON || notification_type == THERAPY_CONTINUED_BY_APP) {
        add_and_send_active_therapy_info(notification_type);
    }
    else{
        add_and_send_notification_info(notification_type);
    }
}

static void on_temperature_update(uint8_t temperature) {
    add_and_send_measurement_info(temperature);
}

static void on_connect_ble() {
    set_ble_connection_status(true);
    add_and_send_device_info();
    NotificationType helmet_status = get_helmet_state() ? HELMET_ON : HELMET_OFF;     //TODO: tam doğru değil.
    add_and_send_notification_info(helmet_status);
}

static void on_disconnect_ble() {
    set_ble_connection_status(false);
    add_and_send_notification_info(BLE_DISCONNECTED);
}

/*
static uint16_t convert_bit_string_to_duration_in_seconds(uint16_t duration_bits) {
    if(duration_bits > 480) {
        ESP_LOGE(TAG, "Wrong therapy duration is got.");
        duration_bits = 480;
    }
    return duration_bits * 5;
}
*/
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
        read_and_set_records(therapy_id);
        ESP_LOGI(TAG, "therapy_id: %d, fragment_count: %d", therapy_id, get_fragment_count());

        for (uint16_t i = 0; i <= get_fragment_count(); i++) {
            const uint8_t* frag = get_fragment(i);
            size_t len = get_fragment_length(i);
            ESP_LOGI(TAG, "fragment id: %d, length: %d", i, len);

            if (frag != NULL && len > 0) {
                ESP_LOGI(TAG, "Sending fragment for therapy_id: %d, fragment_index: %d", therapy_id, i);
                ESP_LOG_BUFFER_HEX(TAG, frag, len);
                send_info_message(RECORDS_INFO_MESSAGE, (uint8_t*)frag, len);
                vTaskDelay(pdMS_TO_TICKS(2000));  // Gerekirse bu süre MTU'ya göre ayarlanabilir
            }
        }
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
    if(msg->type == ACTIVE_THERAPY_INFO_MESSAGE_ACK) {
        if(get_device_state() != STATE_ACTIVE) {
            ESP_LOGE(TAG, "Therapy must have been active.");
        }
        else{
            read_and_set_records(read_therapy_count() - 1);
            
            for (uint16_t i = 0; i <= get_fragment_count(); i++) {
                const uint8_t* frag = get_fragment(i);
                size_t len = get_fragment_length(i);

                if (frag != NULL && len > 0) {
                    send_info_message(RECORDS_INFO_MESSAGE, (uint8_t*)frag, len);
                    vTaskDelay(pdMS_TO_TICKS(20));  // Gerekirse bu süre MTU'ya göre ayarlanabilir
                }
            }
        }
    } else if(msg->type == RECORDS_INFO_FOR_ACTIVE_THERAPY_ACK) {
        add_and_send_measurement_info(get_temperature());
    } else if(msg->type == DEVICE_INFO_MESSAGE_ACK) {
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


void init_ble(){

    init_ble_state_manager();
    
    ESP_ERROR_CHECK(init_bluetooth());
    start_registering_and_advertising();
    register_on_connect_callback(on_connect_ble);
    register_on_write_activation_callback(on_write_of_activation_message);
    register_on_write_feedback_callback(on_write_of_feedback_message);
    register_on_write_updating_records_callback(on_write_of_record_request_message);
    register_on_write_updating_therapy_state_callback(on_write_of_therapy_state);
    register_on_disconnect_callback(on_disconnect_ble);
    register_timer_end_callback(on_timer_end);
    register_timer_start_callback(on_timer_start);
    register_temperature_update(on_temperature_update);
    
}