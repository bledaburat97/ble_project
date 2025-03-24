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
#include "therapy_controller.h"
#include "ble_control.h"
#include "transaction_manager.h"
#include "activation_command_manager.h"
#include "transaction_message_encoder.h"


static const char *TAG = "BLEManager";
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

static void fill_periodic_info(PeriodicInfo* info){
    if (info == NULL) {
        ESP_LOGE(TAG, "Invalid PeriodicInfo");
        return;
    }

    info->therapy_id = 0xFFFF;
    info->remaining_duration = get_last_therapy_applied_duration();
    info->temperature = 0x53;
    info->humidity = 0x14;
}

static void fill_notification_info(NotificationInfo* info, NotificationType notification_type) {
    if (info == NULL) {
        ESP_LOGE(TAG, "Invalid NotificationInfo");
        return;
    }

    info->therapy_id = 0xFFFF;
    info->type = (uint8_t) notification_type;
}

static void ble_notify_task(void *param) {
    while (true) {
        if(get_ble_connection_status()){
            if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                uint8_t periodic_data[SIZE_OF_PERIODIC_INFO];
                PeriodicInfo periodic_info;
                fill_periodic_info(&periodic_info);
                encode_periodic_info(&periodic_info, periodic_data);
                send_periodic_data(periodic_data, sizeof(periodic_data));

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

static void send_aperiodic_info(uint8_t* data, size_t data_length)
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
            // Successfully acquired the mutex
            esp_err_t ret = send_notification_data(data, data_length);

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

static void on_write_of_activation_command(const uint8_t *data, size_t len) {
    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "Byte %d: 0x%02X ('%c')", i, data[i], data[i]);
    }

    ActivationCommand activation_command;
    if(!decode_activation_command(data, len, &activation_command)) {
        return;
    }

    if (activation_command.received_command == 0x00) {
        stop_therapy_timer();
        // TODO: Lazeri kapat
    }
    else {
        ESP_LOGI(TAG, "Therapy ID: %u, Duration: %u",activation_command.therapy_id, activation_command.therapy_duration);
        
        if(can_therapy_start() && activation_command.therapy_duration > 0) {

            for(uint8_t i = 0; i < 6; i++)
            {
                set_brightness_of_region(i + 1, activation_command.region_brightness[i]);
            }

            start_therapy_timer(activation_command.therapy_duration);
            //TODO: turn lasers on
        }
        else {
            ESP_LOGE(TAG, "Therapy couldn't start.");
        }
    }

    for(int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Laser Data received: %d", activation_command.region_brightness[i]);
    }
}

static void on_connect_ble() {
    set_ble_connection_status(true);
    NotificationType helmet_status = get_helmet_status() ? HELMET_ON : HELMET_OFF;
    send_notification(helmet_status);

    if (ble_task_handle == NULL) {
        xTaskCreate(ble_notify_task, "Ble Notify Task", 8192, NULL, 5, &ble_task_handle);
    }
}

static void on_disconnect_ble() {
    set_ble_connection_status(false);
    if (ble_task_handle != NULL) {
        vTaskDelete(ble_task_handle);
        ble_task_handle = NULL;
    }
}

void send_notification(NotificationType notification_type) {
    uint8_t notification_data[SIZE_OF_NOTIFICATION_INFO];
    NotificationInfo notification_info;
    fill_notification_info(&notification_info, notification_type);
    encode_notification_info(&notification_info, notification_data);
    send_aperiodic_info(
        notification_data,
        sizeof(notification_data)
    );

}

void init_ble(){

    ble_mutex = xSemaphoreCreateMutex();
    if (!ble_mutex) {
        ESP_LOGE(TAG, "Failed to create BLE mutex");
        return;
    }
    
    start_registering_and_advertising();
    register_on_connect_callback(on_connect_ble);
    register_on_write_activation_callback(on_write_of_activation_command);
    register_on_disconnect_callback(on_disconnect_ble);
    register_timer_notification_callback(send_notification);
}