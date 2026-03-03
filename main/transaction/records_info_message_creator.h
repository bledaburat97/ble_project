#ifndef RECORDS_INFO_MESSAGE_CREATOR_H
#define RECORDS_INFO_MESSAGE_CREATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void init_records_info_message_creator();
void register_on_change_in_profile_id_during_active(void (*callback)());

#endif