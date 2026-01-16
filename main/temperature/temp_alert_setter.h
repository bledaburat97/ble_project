#ifndef TEMP_ALERT_SETTER_H
#define TEMP_ALERT_SETTER_H

#include <stdint.h>
#include <stdbool.h>


void temp_alert_setter_initialize(bool normal_pin_status);

void temp_alert_setter_register_temperature_alert(void (*callback)(uint8_t));
void temp_alert_setter_register_temperature_normal(void (*callback)(uint8_t));

void temp_alert_setter_monitor_alert_task(void *param);
bool temp_alert_setter_check_alert_status(void);

#endif