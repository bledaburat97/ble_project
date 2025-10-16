#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "matching_message_encoder.h"
#include "device_configuration.h"

#ifndef UNIT_TESTING
#include "esp_log.h"
#else
#include "fake_esp_log.h"
#endif

#define RECORD_TYPE_THERAPY_DURATION 0x01
#define RECORD_TYPE_REMAINING_TIME  0x02
#define RECORD_TYPE_MEASUREMENT     0x03
#define RECORD_TYPE_NOTIFICATION    0x04
#define RECORD_TYPE_BRIGHTNESS      0x05


static const char *TAG = "MatchingMessageEncoder";

uint16_t current_fragment_id = UINT16_MAX;

uint8_t fragments[MAX_FRAGMENT_COUNT][MAX_FRAGMENT_SIZE];
size_t fragment_lengths[MAX_FRAGMENT_COUNT];
static uint16_t fragment_count_index = 0;

static uint16_t fragment_size = DEFAULT_FRAGMENT_SIZE;
static inline uint16_t FRAGMENT_CAPACITY(void) { return fragment_size; }

void fragments_set_capacity(size_t cap) {
    if (cap > MAX_FRAGMENT_SIZE) cap = MAX_FRAGMENT_SIZE;
    fragment_size = cap;
}

void fragments_set_capacity_from_mtu(uint16_t mtu) {
    size_t payload = (mtu > 3) ? (mtu - 3) : 20;
    fragments_set_capacity(payload);
}

static bool start_new_fragment(uint16_t therapy_id) {
    if (FRAGMENT_CAPACITY() < 4) {
        ESP_LOGE(TAG, "Fragment capacity too small (%u)", (unsigned)FRAGMENT_CAPACITY());
        return false;
    }
    ESP_LOGW(TAG, "Fragment Max Capacity: %u", (uint16_t)FRAGMENT_CAPACITY());

    uint32_t next_id = (current_fragment_id == UINT16_MAX) ? 0u
                                                           : (uint32_t)current_fragment_id + 1u;

    if (next_id >= (uint32_t)MAX_FRAGMENT_COUNT) {
        ESP_LOGE(TAG, "Too many fragments (current=%u, next=%lu, max=%u)",
                 (unsigned)current_fragment_id, (unsigned long)next_id, (unsigned)MAX_FRAGMENT_COUNT);
        return false;
    }

    current_fragment_id = (uint16_t)next_id;

    fragments[current_fragment_id][0] = (current_fragment_id >> 8) & 0xFF;
    fragments[current_fragment_id][1] = current_fragment_id & 0xFF;
    fragments[current_fragment_id][2] = (therapy_id >> 8) & 0xFF;
    fragments[current_fragment_id][3] = therapy_id & 0xFF;
    fragment_lengths[current_fragment_id] = 4;
    return true;
}

void start_encoding_for_new_therapy(uint16_t therapy_id, uint16_t therapy_duration, uint16_t passed_therapy_duration) {
    if(!start_new_fragment(therapy_id)) {
        return;
    };
    //ESP_LOGI(TAG, "start_encoding_for_new_therapy: therapy id: %u", therapy_id);

    // Record Type: Fragment Count (0x06)
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0x06;
    fragment_count_index = fragment_lengths[current_fragment_id];
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0; // sonradan doldurulacak.

    // Record Type: Therapy Duration (0x01)
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0x01;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (therapy_duration >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = therapy_duration & 0xFF;

    // Record Type: Passed Therapy Duration (0x02)
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0x02;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (passed_therapy_duration >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = passed_therapy_duration & 0xFF;
}

static void append_records_to_fragment(uint16_t count, uint8_t record_type, size_t record_size, const uint8_t *records, size_t start_index)
{
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = record_type;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (count >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = count & 0xFF;

    //ESP_LOGI(TAG, "Start index: %u", start_index);
    for (size_t i = 0; i < count; ++i) {
        for(int j = 0; j < record_size; j++) {
            //ESP_LOGI(TAG, "Index: %u, Byte: %u", (start_index + i) * record_size + j, records[(start_index + i) * record_size + j]);
        }
        const uint8_t* record_ptr = &records[(start_index + i) * record_size];
        memcpy(&fragments[current_fragment_id][fragment_lengths[current_fragment_id]], record_ptr, record_size);
        fragment_lengths[current_fragment_id] += record_size;
        //ESP_LOGI(TAG, "updated fragment_length: %u", fragment_lengths[current_fragment_id]);
    }
    
}

void encode_records_of_therapy(uint16_t therapy_id, uint8_t record_type,
                               size_t record_size, size_t record_count,
                               const uint8_t *records) {
    const size_t section_header = 1 + 2; // type + count
    const size_t min_required   = section_header + record_size;

    if (FRAGMENT_CAPACITY() < (4 /*frag hdr*/ + min_required)) {
        ESP_LOGE(TAG, "Fragment capacity (%u) too small for record_size=%u",
                 (unsigned)FRAGMENT_CAPACITY(), (unsigned)record_size);
        return;
    }

    size_t index = 0;
    while (record_count > 0) {
        if (((fragments[current_fragment_id][2] << 8) | fragments[current_fragment_id][3]) != therapy_id) {
            ESP_LOGE(TAG, "Encoding for new therapy must have been started.");
            return;
        }

        size_t remaining = FRAGMENT_CAPACITY() - fragment_lengths[current_fragment_id];
        if (remaining < min_required) {

            if(!start_new_fragment(therapy_id)) {
                return;
            }
            remaining = FRAGMENT_CAPACITY() - fragment_lengths[current_fragment_id];
        }

        size_t max_records_here = (remaining - section_header) / record_size;
        uint16_t appending_record_count = (record_count < max_records_here) ? record_count : max_records_here;

        append_records_to_fragment(appending_record_count, record_type, record_size, records, index);

        record_count -= appending_record_count;
        index        += appending_record_count;
    }
}

const uint8_t* get_fragment(uint16_t fragment_id) {
    if (fragment_id >= MAX_FRAGMENT_COUNT) return NULL;
    return fragments[fragment_id];
}

size_t get_fragment_length(uint16_t fragment_id) {
    if (fragment_id >= MAX_FRAGMENT_COUNT) return 0;
    return fragment_lengths[fragment_id];
}

uint16_t get_fragment_count() {
    return current_fragment_id + 1;
}

void add_fragment_count(void) {
    uint16_t total = get_fragment_count();
    fragments[0][fragment_count_index] = (uint8_t)total;
}

void init_fragments()
{
    current_fragment_id = UINT16_MAX;

    for (int i = 0; i < MAX_FRAGMENT_COUNT; i++) {
        fragment_lengths[i] = 0;
        memset(fragments[i], 0, MAX_FRAGMENT_SIZE);
    }
}