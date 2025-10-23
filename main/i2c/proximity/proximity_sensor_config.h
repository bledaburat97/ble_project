#ifndef PROXIMITY_SENSOR_CONFIG_H
#define PROXIMITY_SENSOR_CONFIG_H

#include <stdint.h>

#define COMMAND_REG  0x80
#define PROXIMITY_RATE_REG 0x82
#define IR_LED_CURRENT_REG 0x83
#define PROXIMITY_RESULT_REG_HIGH 0x87
#define PROXIMITY_RESULT_REG_LOW 0x88
#define INTERRUPT_CONTROL_REG 0x89
#define LOW_THRESHOLD_REG_HIGH 0x8A
#define LOW_THRESHOLD_REG_LOW 0x8B
#define HIGH_THRESHOLD_REG_HIGH 0x8C
#define HIGH_THRESHOLD_REG_LOW 0x8D
#define INTERRUPT_STATUS_REG 0x8E

typedef enum {
    CONFIG_LOCK   = (1 << 7),  // Read-only bit
    PROX_DATA_RDY = (1 << 5),  // Read-only: 1 when data is available
    PROX_OD       = (1 << 3),  // On-demand proximity measurement
    PROX_EN       = (1 << 1),  // Enables periodic proximity measurement
    SELFTIMED_EN  = (1 << 0)   // Enables self-timed measurement mode
} CommandRegisterBits;

// Self-timed measurement mode aktif hale getir
#define ENABLE_SELF_TIMED_MODE(byte)  ((byte) | SELFTIMED_EN)

// On-demand ölçüm başlat
#define START_ON_DEMAND_MEASUREMENT(byte)  ((byte) | PROX_OD)

// Periyodik ölçümü etkinleştir
#define ENABLE_PERIODIC_MEASUREMENT(byte)  ((byte) | PROX_EN)

// Bütün ölçümleri devre dışı bırak
#define DISABLE_ALL_MEASUREMENTS(byte)  ((byte) & ~(PROX_OD | PROX_EN | SELFTIMED_EN))

typedef enum {
    PROX_RATE_1_95 = 0x00,  // 1.95 ölçüm/s (DEFAULT
    PROX_RATE_3_90625 = 0x01,  // 3.90625 ölçüm/s
    PROX_RATE_7_8125 = 0x02,  // 7.8125 ölçüm/s
    PROX_RATE_15_625 = 0x03,  // 15.625 ölçüm/s
    PROX_RATE_31_25 = 0x04,  // 31.25 ölçüm/s
    PROX_RATE_62_5 = 0x05,  // 62.5 ölçüm/s
    PROX_RATE_125 = 0x06,  // 125 ölçüm/s
    PROX_RATE_250 = 0x07   // 250 ölçüm/s
} ProximityRate;

typedef struct {
    uint8_t proximity_rate : 3;
    uint8_t reserved : 5;
} ProximityRateRegister;

typedef struct {
    uint8_t ir_led_current : 6;
    uint8_t fuse_prog_id : 2;
} IRLedCurrentRegister;

typedef struct {
    uint8_t int_thres_sel : 1;
    uint8_t int_thres_en : 1;
    uint8_t reserved1 : 1;
    uint8_t int_prox_ready_en : 1;
    uint8_t reserved2 : 1;
    uint8_t int_count_exceed : 3;
} InterruptControlRegister;

typedef enum {
    INT_PROX_READY = (1 << 3),
    INT_TH_LOW = (1 << 1),
    INT_TH_HI = (1 << 0)
} InterruptStatusBits;

typedef struct {
    uint8_t int_th_hi : 1;
    uint8_t int_th_low : 1;
    uint8_t reserved1 : 1;
    uint8_t int_prox_ready : 1;
    uint8_t reserved2 : 4;
} InterruptStatusRegister;

#endif 