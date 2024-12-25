
#include <stdint.h>
#include <stdbool.h>

#ifndef LASER_DRIVER_CONTROL_H
#define LASER_DRIVER_CONTROL_H


#define TOTAL_REGION_COUNT 5 
#define MAX_NUM_OF_LED_OF_LP5036 36


typedef struct {
    uint8_t region_id;
    bool on;
    uint8_t brightness;
} RegionStatusChangedInfo;

typedef struct {
    int regionId;
    int numOfLEDs;
    int ledList[MAX_NUM_OF_LED_OF_LP5036];
    int isBank;
} Region;

typedef struct {
    Region region[TOTAL_REGION_COUNT];
    uint16_t address;
} LP5036Info;

void initialize_laser_driver();
void set_brightness(RegionStatusChangedInfo *region_status_changed_infos, int num_of_changed_regions);
void setDataOfActiveLaserCount(uint8_t* data);
void stop_notification();

#endif 