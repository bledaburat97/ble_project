#include "storage/log_types.h"

#ifndef TIMER_STATE_INFO_MESSAGE_CREATOR_H
#define TIMER_STATE_INFO_MESSAGE_CREATOR_H

void init_timer_state_info_message_creator();
void register_active_or_paused_therapy_info(void (*callback)());
void add_and_send_new_other_state_info(NotificationType notification_type);
void add_and_send_new_therapy_state_info(NotificationType notification_type);
#endif