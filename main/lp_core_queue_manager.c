#include "lp_core_queue_manager.h"

QueueHandle_t lp_core_queue = NULL;

void queue_init() {
    lp_core_queue = xQueueCreate(10, sizeof(lp_core_task_t));
}

BaseType_t queue_add_task(uint32_t command, uint32_t reg, uint32_t value, uint32_t device_address, uint32_t byte_count) {
    if (lp_core_queue == NULL) return pdFAIL;

    lp_core_task_t new_task = {
        .lp_core_command = command,
        .lp_core_register = reg,
        .lp_core_value = value,
        .lp_core_device_address = device_address,
        .lp_core_byte_count = byte_count
    };

    return xQueueSend(lp_core_queue, &new_task, portMAX_DELAY);
}

BaseType_t queue_get_task(lp_core_task_t *task, TickType_t timeout) {
    if (lp_core_queue == NULL) return pdFAIL;

    return xQueueReceive(lp_core_queue, task, timeout);
}