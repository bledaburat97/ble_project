#include <stdint.h>
#include <string.h>

#ifndef MATCHING_MESSAGE_ENCODER_H
#define MATCHING_MESSAGE_ENCODER_H

void start_encoding_for_new_therapy(uint16_t therapy_id, uint16_t therapy_duration, uint16_t remaining_duration);
void encode_records_of_therapy(uint16_t therapy_id, uint8_t record_type, size_t record_size, size_t record_count, const uint8_t *records);
uint16_t get_fragment_count();
const uint8_t* get_fragment(uint16_t fragment_id);
size_t get_fragment_length(uint16_t fragment_id);
void init_fragments();
void fragments_set_capacity(size_t cap);
void fragments_set_capacity_from_mtu(uint16_t mtu);
#endif 