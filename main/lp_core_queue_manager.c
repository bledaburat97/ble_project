#include "lp_core_queue_manager.h"
#include "lp_core_firmware.h"
#include "proximity_sensor_config.h"
#include "proximity_sensor_control.h"
#include "lp_core_main.h"
#include "esp_log.h"

QueueHandle_t lp_core_queue = NULL;
static const char *TAG = "LPCoreQueue";

void initialize_lp_core_queue() {
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

void process_lp_queue_task(void *arg) {
    lp_core_task_t task;

    while (1) {
        uint32_t current_lp_command = __atomic_load_n(&ulp_lp_core_command, __ATOMIC_RELAXED);

        if(current_lp_command == NO_COMMAND){
            if (queue_get_task(&task, pdMS_TO_TICKS(10)) == pdPASS) {
                __atomic_store_n(&ulp_lp_core_command, task.lp_core_command, __ATOMIC_RELAXED);
                __atomic_store_n(&ulp_lp_core_register, task.lp_core_register, __ATOMIC_RELAXED);
                __atomic_store_n(&ulp_lp_core_value, task.lp_core_value, __ATOMIC_RELAXED);
                __atomic_store_n(&ulp_lp_core_device_address, task.lp_core_device_address, __ATOMIC_RELAXED);
                __atomic_store_n(&ulp_lp_core_byte_count, task.lp_core_byte_count, __ATOMIC_RELAXED);

                ESP_LOGI("MAIN", "Queue'dan çıktı: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu",
                         task.lp_core_command, task.lp_core_register, task.lp_core_value, task.lp_core_device_address, task.lp_core_byte_count);
            }
            else {
                // Kuyruk boş ve LP-Core boşta. Görevi kısa bir süre uykuya alarak CPU'yu serbest bırak.
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        else if(current_lp_command == WRITE_COMPLETED) {
            ESP_LOGI(TAG, "Write tamamlandı: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu",
                     current_lp_command,
                     __atomic_load_n(&ulp_lp_core_register, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_value, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_device_address, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_byte_count, __ATOMIC_RELAXED));
            __atomic_store_n(&ulp_lp_core_command, NO_COMMAND, __ATOMIC_RELAXED);
        }
        else if(current_lp_command == READ_COMPLETED) {
            ESP_LOGI(TAG, "Read tamamlandı: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu",
                     current_lp_command,
                     __atomic_load_n(&ulp_lp_core_register, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_value, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_device_address, __ATOMIC_RELAXED),
                     __atomic_load_n(&ulp_lp_core_byte_count, __ATOMIC_RELAXED));
            if(__atomic_load_n(&ulp_lp_core_register, __ATOMIC_RELAXED) == INTERRUPT_STATUS_REG && __atomic_load_n(&ulp_lp_core_device_address, __ATOMIC_RELAXED) == VCNL_3020_ADDRESS) {
                check_interrupt_status(__atomic_load_n(&ulp_lp_core_value, __ATOMIC_RELAXED) & 0xFF, true);
            }
            __atomic_store_n(&ulp_lp_core_command, NO_COMMAND, __ATOMIC_RELAXED);
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}