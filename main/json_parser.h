
#include <stdint.h>

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct {
    uint16_t therapy_id;
    uint32_t therapy_duration;
    int num_of_changed_regions;
    RegionStatusChangedInfo *region_infos;
} TherapyActivationInfo;

TherapyActivationInfo* parse_therapy_activation_info(const char *json_data);

#endif