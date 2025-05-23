#include <stdint.h>
#include <string.h>

#ifndef MATCHING_MESSAGE_ENCODER_H
#define MATCHING_MESSAGE_ENCODER_H

void start_encoding_for_new_therapy(uint16_t therapy_id, uint16_t therapy_duration, uint16_t remaining_duration);
void encode_records_of_therapy(uint16_t therapy_id, uint8_t record_type, size_t record_size, size_t record_count, const uint8_t *records);

#endif 