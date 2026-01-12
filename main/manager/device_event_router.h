#ifndef DEVICE_EVENT_ROUTER_H
#define DEVICE_EVENT_ROUTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "device_events.h"
#include "../storage/log_types.h"
#include "../storage/log_resume.h"
#include "../button/main_button_controller.h"

void init_device_event_router(void);

void post_device_event(const DeviceEvent *event);

void post_button_press_event(ButtonPressType press_type);
void post_timer_completed_event(NotificationType notification_type);
void post_temperature_alert_event(uint8_t sensor_index);
void post_helmet_state_change_event(bool helmet_on);

void post_activation_request_event(const uint8_t *buffer, size_t length);
void post_therapy_state_change_request_event(const uint8_t *buffer, size_t length);

void post_device_start_event();
void post_uncompleted_therapy_set_event(UncompletedTherapyInfo uncompletedTherapyInfo);

#endif
