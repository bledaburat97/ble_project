#include <stdint.h>
#include "stdbool.h"

#ifndef TEMPERATURE_ALERT_CONTROLLER_H
#define TEMPERATURE_ALERT_CONTROLLER_H

void set_alert_pin_normal_status(bool status);
void set_active_temp_sensor_count(uint8_t count);
void monitor_alert_task(void *param);
void initialize_alert_gpios();
bool check_alert_status();
void register_temperature_alert(void (*callback)(uint8_t));
void register_temperature_normal(void (*callback)(uint8_t));
#endif 