#ifndef NOTIFICATION_INFO_MESSAGE_CREATOR_H
#define NOTIFICATION_INFO_MESSAGE_CREATOR_H

#include "storage/log_types.h"

void add_and_send_notification_info(NotificationType notification_type);
void init_notification_info_message_creator();
void send_notification_info(NotificationType type, uint16_t passed_seconds);

#endif