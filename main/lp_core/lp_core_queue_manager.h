#ifndef LP_CORE_QUEUE_MANAGER_H
#define LP_CORE_QUEUE_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

typedef struct {
    uint32_t lp_core_command;
    uint32_t lp_core_register;
    uint32_t lp_core_value;
    uint32_t lp_core_device_address;
    uint32_t lp_core_byte_count;
} lp_core_task_t;

extern QueueHandle_t lp_core_queue;

void initialize_lp_core_queue();
BaseType_t queue_add_task(uint32_t command, uint32_t reg, uint32_t value, uint32_t device_address, uint32_t byte_count);
BaseType_t queue_get_task(lp_core_task_t *task, TickType_t timeout);
void process_lp_queue_task(void *arg);
bool is_all_config_written();

#endif