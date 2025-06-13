#include <stdint.h>
#include "freertos/semphr.h"
#include "log_types.h"

#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

void add_and_send_notification_info(NotificationType notification_type);
void init_ble();

#endif 