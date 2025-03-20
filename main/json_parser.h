
#include <stdint.h>

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct {
    uint8_t received_command;
    uint16_t therapy_id;
    uint32_t therapy_duration;
    int num_of_changed_regions;
    RegionStatusChangedInfo *region_infos;
} TherapyActivationInfo;

TherapyActivationInfo* parse_therapy_activation_info(const char *json_data);

typedef struct {
    uint16_t therapy_id;            // 2 byte
    uint8_t received_command;       // 1 bit
    uint16_t therapy_duration;      // 9 bit
    uint8_t region_brightness[6];   // 6 x 5 = 30 bit
} ActivationInfo;

typedef struct {
    uint16_t therapy_id;            // 2 byte
    uint16_t remaining_duration;    // 9 bit
    uint8_t temperature;            // 7 bit
    uint8_t humidity;               // 5 bit
} PeriodicInfo;

void parse_activation_info(const uint8_t *data, size_t len, ActivationInfo *therapy_info);
void encode_periodic_info(const PeriodicInfo *info, uint8_t *output);
#endif