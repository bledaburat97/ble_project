#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "matching_message_encoder.h"

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
#define MAX_FRAGMENT_SIZE 30
#define MAX_FRAGMENT_COUNT 100

static const char *TAG = "MatchingMessageEncoder";

uint16_t current_fragment_id = UINT16_MAX;

uint8_t fragments[MAX_FRAGMENT_COUNT][MAX_FRAGMENT_SIZE];
size_t fragment_lengths[MAX_FRAGMENT_COUNT];

static void start_new_fragment(uint16_t therapy_id) {
    current_fragment_id++;
    ESP_LOGI(TAG, "updated current_fragment_id: %u", current_fragment_id);
    if(current_fragment_id >= MAX_FRAGMENT_COUNT) {
        ESP_LOGE(TAG, "Maximum number of fragment is created.");
        //ASSERT
        return;
    }
    fragments[current_fragment_id][0] = (current_fragment_id >> 8) & 0xFF;
    fragments[current_fragment_id][1] = current_fragment_id & 0xFF;
    fragments[current_fragment_id][2] = (therapy_id >> 8) & 0xFF;
    fragments[current_fragment_id][3] = therapy_id & 0xFF;
    fragment_lengths[current_fragment_id] = 4;
}

void start_encoding_for_new_therapy(uint16_t therapy_id, uint16_t therapy_duration, uint16_t remaining_duration) {
    start_new_fragment(therapy_id);
    ESP_LOGI(TAG, "start_encoding_for_new_therapy: therapy id: %u", therapy_id);

    // Record Type: Therapy Duration (0x01)
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0x01;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (therapy_duration >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = therapy_duration & 0xFF;

    // Record Type: Remaining Time (0x02)
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = 0x02;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (remaining_duration >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = remaining_duration & 0xFF;
}

static void append_records_to_fragment(uint16_t count, uint8_t record_type, size_t record_size, const uint8_t *records, size_t start_index)
{
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = record_type;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = (count >> 8) & 0xFF;
    fragments[current_fragment_id][fragment_lengths[current_fragment_id]++] = count & 0xFF;

    ESP_LOGI(TAG, "Start index: %u", start_index);
    for (size_t i = 0; i < count; ++i) {
        for(int j = 0; j < record_size; j++) {
            //ESP_LOGI(TAG, "Index: %u, Byte: %u", (start_index + i) * record_size + j, records[(start_index + i) * record_size + j]);
        }
        const uint8_t* record_ptr = &records[(start_index + i) * record_size];
        memcpy(&fragments[current_fragment_id][fragment_lengths[current_fragment_id]], record_ptr, record_size);
        fragment_lengths[current_fragment_id] += record_size;
        ESP_LOGI(TAG, "updated fragment_length: %u", fragment_lengths[current_fragment_id]);
    }
    
}

void encode_records_of_therapy(uint16_t therapy_id, uint8_t record_type, size_t record_size, size_t record_count, const uint8_t *records) {
    size_t index = 0;

    while(record_count > 0) {
        ESP_LOGI(TAG, "record_count: %u.", record_count);

        if(((fragments[current_fragment_id][2] << 8) | fragments[current_fragment_id][3]) != therapy_id) {
            ESP_LOGE(TAG, "Encoding for new therapy must have been started.");
            //ASSERT
            return;
        }

        size_t header_size = 1 + 2;

        size_t remaining = MAX_FRAGMENT_SIZE - fragment_lengths[current_fragment_id];
        ESP_LOGI(TAG, "current fragment length: %u, remaining: %u", fragment_lengths[current_fragment_id], remaining);

        if (remaining < header_size + record_size) {
            ESP_LOGI(TAG, "start new fragment.");
            start_new_fragment(therapy_id);
            remaining = MAX_FRAGMENT_SIZE - fragment_lengths[current_fragment_id];
            ESP_LOGI(TAG, "new fragment length: %u, remaining: %u", fragment_lengths[current_fragment_id], remaining);
        }

        size_t max_records_here = (remaining - header_size) / record_size;
        uint16_t appending_record_count = (record_count < max_records_here) ? record_count : max_records_here;

        append_records_to_fragment(appending_record_count, record_type, record_size, records, index);
        ESP_LOGI(TAG, "append_records_to_fragment: appending_record_count: %u, record_type: %u, record_size: %u, index: %u, record_count: %u"
             ,appending_record_count, record_type, record_size, index, record_count);

        ESP_LOGI(TAG, "current fragment length: %u", fragment_lengths[current_fragment_id]);


        record_count -= appending_record_count;
        index += appending_record_count;
    }
    
    ESP_LOGI(TAG, "All records are appended.");
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

void init_fragments()
{
    current_fragment_id = UINT16_MAX;

    for (int i = 0; i < MAX_FRAGMENT_COUNT; i++) {
        fragment_lengths[i] = 0;
        memset(fragments[i], 0, MAX_FRAGMENT_SIZE);
    }
}