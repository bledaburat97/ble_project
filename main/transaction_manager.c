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


static const char *TAG = "BLEManager";
SemaphoreHandle_t ble_mutex = NULL;
static bool ble_connection_status = false;

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
        ESP_LOGE("BLE", "Invalid PeriodicInfo");
        return;
    }

    info->therapy_id = 0xFFFF;
    info->remaining_duration = get_last_therapy_applied_duration();
    info->temperature = 0x53;
    info->humidity = 0x14;
}

static void ble_notify_task(void *param) {
    while (true) {
        if(get_ble_connection_status()){
            if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                uint8_t periodic_data[5];
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



static void on_write_of_activation_info(const uint8_t *data, size_t len) {
    restart_inactivity_timer();
    // Validate input data
    if (len == 0) {
        ESP_LOGE(TAG, "No data received for Laser Control");
        return;
    }

    // Parse incoming data
    char *data_copy = (char *)malloc(len + 1);
    if (data_copy == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return;
    }
    memcpy(data_copy, data, len);
    data_copy[len] = '\0';
    
    ESP_LOGI(TAG, "Received data (length: %d):", len);

    for (int i = 0; i < len; i++) {
        ESP_LOGI(TAG, "Byte %d: 0x%02X ('%c')", i, data[i], data[i]);
    }

    if (len >= 3 && len <= 9) {
        ESP_LOGI(TAG, "Write len: %u", len);
        ActivationInfo activation_info;
        parse_activation_info(data, len, &activation_info);

        if (activation_info.received_command == 0x00) {
            stop_therapy_timer();
            // TODO: Lazeri kapat
        }
        else {
            ESP_LOGI(TAG, "Parsed Therapy ID: %u, Duration: %u",activation_info.therapy_id, activation_info.therapy_duration);
                
            if(can_therapy_start()) {
                /*
                if (therapy_activation_info->region_infos != NULL && therapy_activation_info->num_of_changed_regions > 0) {
                    set_brightness(therapy_activation_info->region_infos, therapy_activation_info->num_of_changed_regions);
                    ESP_LOGI(TAG, "Brightness updated successfully.");
                }

                else
                {
                    ESP_LOGW(TAG, "No regions to update.");
                }
                    */
                start_therapy_timer(activation_info.therapy_duration);
                //TODO: turn lasers on
            }
            else{
                ESP_LOGE(TAG, "Therapy couldn't start.");
            }
        }

        for(int i = 0; i < 6; i++) {
            ESP_LOGI(TAG, "Laser Data received: %d", activation_info.region_brightness[i]);
        }


    } else {
        ESP_LOGE(TAG, "Invalid data received, length: %d", len);
    }
}

static void on_connect_ble() {
    set_ble_connection_status(true);
    NotificationType helmet_status = get_helmet_status() ? HELMET_ON : HELMET_OFF;
    send_notification(helmet_status);

    xTaskCreate(ble_notify_task, "Ble Notify Task", 8192, NULL, 5, NULL);
}

static void on_disconnect_ble() {
    set_ble_connection_status(false);
}

void send_notification(NotificationType notification_type) {
    uint8_t notification_data = (uint8_t)notification_type;
    send_aperiodic_info(
        &notification_data,
        sizeof(notification_data)
    );

    ESP_LOGI("Main", "Notification sent: 0x%02X", notification_data);
}

void init_ble(){

    ble_mutex = xSemaphoreCreateMutex();
    if (!ble_mutex) {
        ESP_LOGE(TAG, "Failed to create BLE mutex");
        return;
    }
    
    start_registering_and_advertising();
    register_on_connect_callback(on_connect_ble);
    register_on_write_activation_callback(on_write_of_activation_info);
    register_on_disconnect_callback(on_disconnect_ble);
    register_timer_notification_callback(send_notification);
}