#ifndef BLE_CONTROLLER_H
#define BLE_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

typedef enum {
    RECORDS_INFO_MESSAGE,
    TIMER_STATE_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE,
} MessageType;

void init_ble(void);

esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length);

void register_on_connect_callback(void (*callback)(void));
void register_on_disconnect_callback(void (*callback)(void));
void register_on_write_activation_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_profile_info_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_updating_therapy_state_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_record_request_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_updating_passkey_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_updating_configuration_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_wifi_config_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_dynamic_period_change_callback(void (*callback)(uint16_t));

void set_send_period_ms(uint16_t ms);

bool ble_wait_for_indication_conf(esp_gatt_status_t *out_status, uint32_t timeout_ms);

//TRY_IN_FUTURE
/*
void request_conn_interval_ms(uint16_t target_ms);     
esp_err_t set_phy_2m(void);
esp_err_t set_phy_1m(void);
esp_err_t set_phy_coded_s2(void);
esp_err_t set_phy_coded_s8(void);
esp_err_t set_phy_coded_any(void);
*/

#endif