#ifndef LP_CORE_MAIN_H
#define LP_CORE_MAIN_H

#include <stdint.h>
#include "ulp_lp_core.h"

#ifndef RTC_SLOW_ATTR
#define RTC_SLOW_ATTR __attribute__((section(".rtc.data")))
#endif

#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR __attribute__((section(".rtc.data")))
#endif

typedef enum {
    NO_COMMAND = 0,
    WRITE_COMMAND = 1,
    READ_COMMAND = 2,
    WRITE_COMPLETED = 3,
    READ_COMPLETED = 4
} LpI2COperations;

void initialize_lp_core(void);


extern const uint8_t lp_core_main_bin_start[] asm("_binary_lp_core_firmware_bin_start");
extern const uint8_t lp_core_main_bin_end[] asm("_binary_lp_core_firmware_bin_end");

#endif // LP_CORE_MAIN_H