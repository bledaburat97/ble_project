#ifndef RECORDS_INFO_MESSAGE_CREATOR_H
#define RECORDS_INFO_MESSAGE_CREATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void init_records_info_message_creator();
void send_records_info_message(uint16_t therapy_id, bool is_active_therapy);
#endif