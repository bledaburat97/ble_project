#include <stdint.h>
#include "freertos/semphr.h"
#include "notification.h"

#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

void send_notification(NotificationType notification_type);
void send_notification_in_json(NotificationType notification_type);
void init_ble();

#endif 