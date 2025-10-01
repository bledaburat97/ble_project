//can be deleted

#include "therapy_message_counter.h"

#include <stdint.h>

static uint16_t message_id;

uint16_t get_message_id() {
    message_id++;
    return message_id;
}

//URGENT terapi başlayınca çağır.
void reset_message_id() {
    message_id = 0;
}
