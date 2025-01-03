#include <stdint.h>
#include "freertos/semphr.h"

#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#define PROFILE_A_APP_ID 0
#define GATTS_SERVICE_UUID       0x1800  // Custom service UUID
#define GATTS_CHAR_UUID_LASER_CONTROL   0x2A56
#define GATTS_CHAR_UUID_PROXIMITY    0x2A57
#define GATTS_CHAR_UUID_TEMPERATURE  0x2A58

#define NOTIFICATION_INFO_CHAR_HANDLE  0x002A
#define TEMPERATURE_INFO_CHAR_HANDLE  0x002B
#define HUMIDITY_INFO_CHAR_HANDLE  0x002C
#define LAST_THERAPY_INFO_CHAR_HANDLE 0x002D
#define ACTIVATION_INFO_CHAR_HANDLE 0x002E
#define LASER_CTRL_CHAR_HANDLE  0x002F

void ble_notify_task(void *param);
void send_aperiodic_info(uint16_t char_handle, uint8_t* data, size_t data_length);
void init_ble();
extern SemaphoreHandle_t ble_mutex;
#endif 



