
#include <stdint.h>

#ifndef ACTIVATION_COMMAND_MANAGER_H
#define ACTIVATION_COMMAND_MANAGER_H

#define ACTIVATION_COMMAND_BYTE_COUNT 7

typedef struct {
    uint16_t therapy_id;            // 2 byte
    uint8_t received_command;       // 1 bit
    uint16_t therapy_duration;      // 9 bit
    uint8_t region_brightness[6];   // 6 x 5 = 30 bit
} ActivationCommand;

bool decode_activation_command(const uint8_t *data, size_t len, ActivationCommand *activation_command);
#endif