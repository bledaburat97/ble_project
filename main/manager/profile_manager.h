#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


void set_profile_info(uint32_t profile_id, bool is_profile_changed, bool is_first_profile);
void try_append_profile(uint16_t therapy_id);

#endif