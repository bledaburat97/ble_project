#include <stdint.h>
#include <stdbool.h>

#ifndef GENERAL_MANAGER_H
#define GENERAL_MANAGER_H

void change_helmet_state(bool helmet_state);
void init_general_manager();
void start_device();
void try_start_new_therapy_by_activation(uint16_t duration);

#endif