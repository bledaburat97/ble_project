#ifndef TEMPERATURE_SENSOR_CONFIG_H
#define TEMPERATURE_SENSOR_CONFIG_H

#include <stdint.h>

// OS - One Shot Conversion Bit
typedef enum {
    OS_DISABLED = 0,
    OS_ENABLED = 1
} OS_Mode;

// CR1, CR0 - Conversion Rate Selection Bits
typedef enum {
    CR_8HZ = 0b00,  // 8Hz / 0.125s (default)
    CR_4HZ = 0b01,  // 4Hz / 0.25s
    CR_1HZ = 0b10,  // 1Hz / 1s
    CR_0_25HZ = 0b11 // 0.25Hz / 4s
} ConversionRate;

// F1, F0 - Fault Queue Bits
typedef enum {
    FAULT_QUEUE_1 = 0b00,  // Default
    FAULT_QUEUE_2 = 0b01,
    FAULT_QUEUE_4 = 0b10,
    FAULT_QUEUE_6 = 0b11
} FaultQueue;

// POL - Alert Output Polarity Bit
typedef enum {
    ALERT_ACTIVE_LOW = 0,   // Default
    ALERT_ACTIVE_HIGH = 1
} AlertPolarity;

// ALTM - Alert Mode Bit
typedef enum {
    ALERT_COMPARATOR_MODE = 0, // Default
    ALERT_INTERRUPT_MODE = 1
} AlertMode;

// SD - Shutdown Bit
typedef enum {
    SHUTDOWN_MODE = 1,
    NORMAL_MODE = 0
} ShutdownMode;

// Struct for Configuration Register
typedef struct {
    ShutdownMode sd : 1;        // Bit 0
    AlertMode altm : 1;         // Bit 1
    AlertPolarity pol : 1;      // Bit 2
    FaultQueue fq : 2;          // Bit 4-3
    ConversionRate cr : 2;      // Bit 6-5
    OS_Mode os : 1;             // Bit 7
} TempSensorConfigReg;

#endif // TEMPERATURE_SENSOR_CONFIG_H