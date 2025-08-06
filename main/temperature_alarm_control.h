#include <stdint.h>
#include "stdbool.h"

#ifndef TEMPERATURE_ALARM_CONTROL_H
#define TEMPERATURE_ALARM_CONTROL_H

void set_alert_pin_normal_status(bool status);
void set_alarm_gpios(const uint8_t* alarm_gpios, uint8_t count);
void monitor_alert_task(void *param);
void initialize_alert_gpios();
bool check_alert_status();
void register_temperature_alert(void (*callback)(uint8_t));
void register_temperature_normal(void (*callback)(uint8_t));
#endif 