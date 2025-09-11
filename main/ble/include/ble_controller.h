#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

// Dışarıya açılan tipler (projen zaten tanımlıyorsa buradan kaldır)
typedef enum {
    RECORDS_INFO_MESSAGE,
    TIMER_STATE_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE,
} MessageType;

// Dış API (mevcut imzaları koruyoruz)
esp_err_t init_bluetooth(void);
esp_err_t start_registering_and_advertising(void);

esp_err_t ble_send_info_message_with_type(MessageType message_type, uint8_t* data, size_t data_length);

void register_on_connect_callback(void (*callback)(void));
void register_on_disconnect_callback(void (*callback)(void));
void register_on_write_activation_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_updating_records_callback(void (*callback)(const uint8_t *buf, size_t len));
//void register_on_write_feedback_callback(void (*callback)(const char*));
void register_on_write_updating_therapy_state_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_records_feedback_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_on_write_updating_passkey_callback(void (*callback)(const uint8_t *buf, size_t len));
void register_dynamic_period_change_callback(void (*callback)(uint16_t));

void request_conn_interval_ms(uint16_t target_ms);
esp_err_t set_phy_2m(void);
esp_err_t set_phy_1m(void);
esp_err_t set_phy_coded_s2(void);
esp_err_t set_phy_coded_s8(void);
esp_err_t set_phy_coded_any(void);

// Başka yerde tanımlı yardımcılar (mevcut kodunda zaten "extern" kullanıyorsun)
void set_send_period_ms(uint16_t ms);
void set_bundle_size(uint8_t n);
void use_indication(bool on);
void use_indication_for_critical(bool on);

bool ble_wait_for_indication_conf(esp_gatt_status_t *out_status, uint32_t timeout_ms);
