#include <stdint.h>
#include "freertos/semphr.h"
#include "notification.h"

#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#define PROFILE_A_APP_ID 0
#define GATTS_CHAR_UUID_NOTIFICATION   0x2A56
#define GATTS_CHAR_UUID_TEMPERATURE    0x2A57
#define GATTS_CHAR_UUID_HUMIDITY       0x2A58
#define GATTS_CHAR_UUID_LAST_THERAPY   0x2A59
#define GATTS_CHAR_UUID_ACTIVATION     0x2A5A
#define GATTS_CHAR_UUID_LASER_CONTROL  0x2A5B

void ble_notify_task(void *param);
void send_aperiodic_info(uint16_t char_handle, uint8_t* data, size_t data_length);
void init_ble();
uint16_t get_last_therapy_handle();
uint16_t get_temperature_handle();
uint16_t get_notification_handle();
void send_notification(NotificationType notification_type);
#endif 



