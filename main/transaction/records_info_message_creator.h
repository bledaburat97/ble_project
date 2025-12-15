#ifndef RECORDS_INFO_MESSAGE_CREATOR_H
#define RECORDS_INFO_MESSAGE_CREATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void init_records_info_message_creator();
void on_write_of_record_request_message(const uint8_t *buf, size_t len);
void send_records_info_message(uint16_t therapy_id);
#endif