#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "driver/gpio.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_9
#define RGB_LED_GPIO GPIO_NUM_15
#define TEMP_SENSOR_ALERT_GPIO  GPIO_NUM_11

extern gpio_num_t laser_driver_gpios[];
#endif
