#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_log.h"

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

static const char *TAG = "TransactionManager";
SemaphoreHandle_t ble_mutex = NULL;
static bool ble_connection_status = false;
static TaskHandle_t ble_task_handle = NULL;

static void set_ble_connection_status(bool status) {
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ble_connection_status = status;
        ESP_LOGI(TAG, "ble_connection_status updated to: %s", status ? "true" : "false");
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status update");
    }
}

bool get_ble_connection_status() {
    bool status = false;
    if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = ble_connection_status;
        xSemaphoreGive(ble_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire BLE mutex for ble_connection_status read");
    }
    return status;
}

static void send_info_message(uint8_t* data, size_t data_length)
{
    if (!get_ble_connection_status()) {
        ESP_LOGW(TAG, "No active BLE connection, cannot send aperiodic info.");
        return;
    }

    int retry_count = 0;
    const int max_retries = 2;  // Set the maximum number of retries
    const int retry_delay_ms = 100;  // Delay between retries in milliseconds

    while (retry_count < max_retries) {
        if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            esp_err_t ret = send_data_with_ble(data, data_length);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Successfully sent aperiodic info.");
            } else {
                ESP_LOGE(TAG, "Failed to send aperiodic info: %s", esp_err_to_name(ret));
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

static void send_device_info_message() {
    DeviceInfoMessage message;
    //message.current_time; //TODO: set current time when RTC integrated.

    message.type = BLE_CONNECTED;
    esp_read_mac(message.device_id, ESP_MAC_WIFI_STA);
    message.passed_seconds = get_passed_duration();
    
    char *json_str = encode_device_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message((uint8_t*)json_str, len);

    free(json_str);
}

static void send_therapy_start_info_message(NotificationType type, uint16_t therapy_duration, uint16_t therapy_id, uint16_t passed_seconds) {
    TherapyStartInfoMessage message;
    message.type = type;
    message.therapy_dur = therapy_duration;
    message.therapy_id = therapy_id;
    message.passed_seconds = passed_seconds;
    
    char *json_str = encode_therapy_start_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message((uint8_t*)json_str, len);

    free(json_str);
}

static void send_measurement_info_message(uint8_t temperature, uint8_t humidity) {
    MeasurementInfoMessage message;
    message.type = MEASUREMENT_CHANGED;
    message.temperature = temperature;
    message.humidity = humidity;
    message.passed_seconds = get_passed_duration();
    
    char *json_str = encode_measurement_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message((uint8_t*)json_str, len);

    free(json_str);
}

static void send_notification_message(NotificationType type) {
    NotificationMessage message;
    message.type = type;
    message.passed_seconds = get_passed_duration();
    
    char *json_str = encode_notification_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message((uint8_t*)json_str, len);

    free(json_str);  // cJSON_PrintUnformatted ile heap'e alındığı için temizlenmeli
}

static void add_and_send_therapy_start(NotificationType notification_type) {
    uint16_t therapy_id = 0; //URGENT
    uint16_t therapy_duration = 0; //URGENT
    uint16_t passed_seconds = get_passed_duration();
    uint8_t data[] = {therapy_id >> 8, therapy_id & 0xFF, therapy_duration >> 8, therapy_duration & 0xFF, passed_seconds >> 8, passed_seconds & 0xFF};
    add_log(notification_type, data, sizeof(data), passed_seconds);
    send_therapy_start_info_message(notification_type, therapy_id, therapy_duration, passed_seconds);
}

static void add_and_send_measurement_info() {
    uint8_t temperature = 0; //URGENT
    uint8_t humidity = 0; //URGENT
    uint8_t data[] = {temperature, humidity}; //URGENT eğer temp veya hum değişmişse.
    uint16_t passed_seconds = get_passed_duration();
    add_log(MEASUREMENT_CHANGED, data, sizeof(data), passed_seconds);
    send_measurement_info_message(temperature, humidity);
}

void add_and_send_notification(NotificationType notification_type) {

    if(notification_type == THERAPY_STARTED_BY_BUTTON || notification_type == THERAPY_STARTED_BY_APP
         || notification_type == THERAPY_CONTINUED_BY_BUTTON || notification_type == THERAPY_CONTINUED_BY_APP) {
            add_and_send_therapy_start(notification_type);
    }
    else if (notification_type == BLE_CONNECTED) {
        uint16_t passed_seconds = get_passed_duration();
        add_notification_log(BLE_CONNECTED, passed_seconds);
        send_device_info_message();
    }
    else{
        uint16_t passed_seconds = get_passed_duration();
        add_notification_log(notification_type, passed_seconds);
        send_notification_message(notification_type);
    }

}

static void ble_notify_task(void *param) {
    while (true) {
        if(get_ble_connection_status()){
            if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                add_and_send_measurement_info();
                // Release the mutex for other tasks
                xSemaphoreGive(ble_mutex);
            } 
            else {
                ESP_LOGW(TAG, "Failed to obtain BLE mutex within 1 second");
            }
        }
        else 
        {
            ESP_LOGW(TAG, "BLE disconnected, stopping notify task...");
            vTaskDelay(pdMS_TO_TICKS(100));
            vTaskDelete(NULL);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void on_connect_ble() {
    set_ble_connection_status(true);
    add_and_send_notification(BLE_CONNECTED);
    NotificationType helmet_status = get_helmet_state() ? HELMET_ON : HELMET_OFF;     //TODO: tam doğru değil.
    add_and_send_notification(helmet_status);

    if (ble_task_handle == NULL) {
        xTaskCreate(ble_notify_task, "Ble Notify Task", 8192, NULL, 5, &ble_task_handle);
    }
}

static void on_disconnect_ble() {
    set_ble_connection_status(false);
    add_and_send_notification(BLE_DISCONNECTED);
    if (ble_task_handle != NULL) {
        vTaskDelete(ble_task_handle);
        ble_task_handle = NULL;
    }
}

static uint16_t convert_bit_string_to_duration_in_seconds(uint16_t duration_bits) {
    if(duration_bits > 480) {
        ESP_LOGE(TAG, "Wrong therapy duration is got.");
        duration_bits = 480;
    }
    return duration_bits * 5;
}

static void on_write_of_activation_command(const uint8_t *data, size_t len) {
    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "Byte %d: 0x%02X ('%c')", i, data[i], data[i]);
    }

    ActivationCommand activation_command;
    if(!decode_activation_command(data, len, &activation_command)) {
        return;
    }

    if (activation_command.received_command == 0x00) {
        if (get_device_state() == STATE_ACTIVE) {
            add_and_send_measurement_info(THERAPY_STOPPED_BY_APP);
            start_inactivity_timer();
        }
        // TODO: Lazeri kapat
    }
    else {
        ESP_LOGI(TAG, "Therapy ID: %u, Duration: %u",activation_command.therapy_id, activation_command.therapy_duration);
        
        if(activation_command.therapy_duration > 0)
        {
            if (get_device_state() == STATE_INACTIVITY || get_device_state() == STATE_ACTIVE) {
                if (get_helmet_state() == true) {
                    for(uint8_t i = 0; i < 6; i++)
                    {
                        set_brightness_of_region(i + 1, activation_command.region_brightness[i]);
                    }
                    uint16_t duration = convert_bit_string_to_duration_in_seconds(activation_command.therapy_duration);
                    start_new_therapy(duration);
                }
                else {
                    ESP_LOGE(TAG, "Therapy couldn't start.");
                    //TODO: error notification
                }
            }
            else {
                ESP_LOGE(TAG, "Therapy couldn't start.");
                //TODO: error notification
            }
        }
    }

    for(int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Laser Data received: %d", activation_command.region_brightness[i]);
    }
}

void init_ble(){

    ble_mutex = xSemaphoreCreateMutex();
    if (!ble_mutex) {
        ESP_LOGE(TAG, "Failed to create BLE mutex");
        return;
    }
    
    ESP_ERROR_CHECK(init_bluetooth());
    start_registering_and_advertising();
    register_on_connect_callback(on_connect_ble);
    register_on_write_activation_callback(on_write_of_activation_command);
    register_on_disconnect_callback(on_disconnect_ble);
    register_timer_notification_callback(add_and_send_notification);
}