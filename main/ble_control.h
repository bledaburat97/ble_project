#include <stdint.h>
#include "freertos/semphr.h"

#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#define PROFILE_A_APP_ID 0
#define GATTS_CHAR_UUID_RECORDS                 0x2A56
#define GATTS_CHAR_UUID_STARTING_THERAPY        0x2A57
#define GATTS_CHAR_UUID_MEASUREMENT             0x2A58
#define GATTS_CHAR_UUID_NOTIFICATION            0x2A59
#define GATTS_CHAR_UUID_DEVICE                  0x2A5A
#define GATTS_CHAR_UUID_ACTIVATION              0x2A5B
#define GATTS_CHAR_UUID_UPDATING_RECORDS        0x2A5C
#define GATTS_CHAR_UUID_FEEDBACK                0x2A5D
#define GATTS_CHAR_UUID_UPDATING_THERAPY_STATE  0x2A5E

#define GATTS_SERVICE_UUID16           0x5555

typedef enum {
    RECORDS_INFO_MESSAGE,
    ACTIVE_THERAPY_INFO_MESSAGE,
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