#ifndef TIMER_STATE_INFO_MESSAGE_CREATOR_H
#define TIMER_STATE_INFO_MESSAGE_CREATOR_H

#include "storage/log_types.h"

void init_timer_state_info_message_creator();
void add_and_send_new_other_state_info(NotificationType notification_type);
void send_new_therapy_started(NotificationType notification_type);
void send_therapy_continued(NotificationType notification_type);

#endif