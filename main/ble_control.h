#include <stdint.h>
#include "log_types.h"
#include "esp_err.h"

#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

typedef enum {
    RECORDS_INFO_MESSAGE,
    TIMER_STATE_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE
} MessageType;

esp_err_t init_bluetooth();
esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length);
void register_on_connect_callback(void (*callback)());
void register_on_disconnect_callback(void (*callback)());
void register_on_write_activation_callback(void (*callback)(const char*));
void register_on_write_updating_records_callback(void (*callback)(const char*));
void register_on_write_feedback_callback(void (*callback)(const char*));
void register_on_write_updating_therapy_state_callback(void (*callback)(const char*));
esp_err_t start_registering_and_advertising();



#endif 