#ifndef LASER_DRIVER_CONTROLLER_H
#define LASER_DRIVER_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "../../device_configuration.h"

typedef struct {
    uint8_t region_id;
    uint64_t led_list;
    bool is_bank;
} RegionPiece;

typedef struct {
    uint8_t region_piece_count;
    RegionPiece region_piece_list[TOTAL_REGION_COUNT];
    uint8_t address;
} LaserDriverInfo;

typedef enum {
    LED_GLOBAL_OFF = 0x00,
    MAX_CURRENT_OPTION = 0x01,
    PWM_DITHERING_EN = 0x02,
    AUTO_INCR_EN = 0x03,
    POWER_SAVE_EN = 0x04,
    LOG_SCALE_EN = 0x05
} DeviceConfig1UpdateType;

void initialize_laser_driver_gpio();
void set_brightness_of_region(uint8_t region_id, uint8_t brightness_percentage);
void initialize_laser_drivers();
void update_device_config1(bool status, DeviceConfig1UpdateType type);
void set_laser_drivers_gpio_pin_status(bool status);
void set_laser_drivers_status(bool status);
#endif 