#include "i2c_control.h"
#include "laser_driver_control.h"

#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "sdkconfig.h"

#define MAX_BRIGHTNESS 0xFF

void app_main() {
    initialize_i2c();
    initialize_laser_driver();
}


void activate_lasers(uint8_t mode) {
    //make HIGH EN pin of lp5036s.
    initialize_laser_drivers();
    RegionStatusChangedInfo region_list[] = {
        {
            .brightness = MAX_BRIGHTNESS,
            .on = true,
            .region_id = 1
        },
        {
            .brightness = MAX_BRIGHTNESS,
            .on = true,
            .region_id = 2
        },
        {
            .brightness = MAX_BRIGHTNESS,
            .on = true,
            .region_id = 3
        },
        {
            .brightness = MAX_BRIGHTNESS,
            .on = true,
            .region_id = 4
        }
    };

    set_brightness(region_list, 4);
}

void stop_lasers() {
    stop_laser_drivers();
}
