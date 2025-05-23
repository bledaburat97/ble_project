#include <stdint.h>
#include "freertos/semphr.h"

#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#define PROFILE_A_APP_ID 0
#define GATTS_CHAR_UUID_PERIODIC       0x2A56
#define GATTS_CHAR_UUID_NOTIFICATION   0x2A57
#define GATTS_CHAR_UUID_ACTIVATION     0x2A58
#define GATTS_SERVICE_UUID16           0x5555

esp_err_t init_bluetooth();
esp_err_t send_periodic_data(uint8_t* data, size_t data_length);
esp_err_t send_data_with_ble(uint8_t* data, size_t data_length);
void register_on_connect_callback(void (*callback)());
void register_on_disconnect_callback(void (*callback)());
void register_on_write_activation_callback(void (*callback)(const uint8_t*, size_t));
esp_err_t start_registering_and_advertising();

#endif 