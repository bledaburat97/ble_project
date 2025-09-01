#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RECORDS_INFO_MESSAGE = 0,
    TIMER_STATE_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE
} MessageType;

esp_err_t init_bluetooth(void);
esp_err_t start_registering_and_advertising(void);
esp_err_t ble_stop(void);
esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t *data, size_t data_length);

esp_err_t set_phy_2m(void);
esp_err_t set_phy_1m(void);
esp_err_t set_phy_coded_s2(void);
esp_err_t set_phy_coded_s8(void);
esp_err_t set_phy_coded_any(void);


void request_conn_interval_ms(uint16_t target_ms);
void register_on_connect_callback(void (*callback)(void));
void register_on_disconnect_callback(void (*callback)(void));
void register_on_write_activation_callback(void (*callback)(const char*));
void register_on_write_updating_records_callback(void (*callback)(const char*));
void register_on_write_feedback_callback(void (*callback)(const char*));
void register_on_write_updating_therapy_state_callback(void (*callback)(const char*));
void register_on_write_records_feedback_callback(void (*callback)(const char*));

void register_dynamic_period_change_callback(void (*callback)(uint16_t));

#ifdef __cplusplus
}
#endif

#endif