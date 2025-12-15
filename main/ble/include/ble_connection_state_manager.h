#ifndef BLE_CONNECTION_STATE_MANAGER_H
#define BLE_CONNECTION_STATE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void init_ble_state_manager();
void set_ble_connection_status(bool status);
bool get_ble_connection_status();
SemaphoreHandle_t get_ble_mutex_handle();

#endif