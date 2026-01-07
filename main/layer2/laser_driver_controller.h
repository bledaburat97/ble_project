#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOTAL_REGION_COUNT 6

void init_laser_driver(void);

void set_brightness_of_region(uint8_t region_id, uint8_t brightness_percent);

void set_brightness_of_all_regions(uint8_t brightness_percent);

void set_laser_drivers_status(bool enabled);

typedef struct {
    uint8_t region_id;
    uint64_t led_list;
} RegionPiece;

typedef struct {
    uint8_t region_piece_count;
    RegionPiece region_piece_list[TOTAL_REGION_COUNT];
    uint8_t address;
} LaserDriverInfo;

#ifdef __cplusplus
}
#endif
