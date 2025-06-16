
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "timer_management.h"
#include "transaction_manager.h"
#include "json_encoder.h"

const int BOOT_BUTTON_GPIO = GPIO_NUM_9;
static QueueHandle_t button_queue;
static bool timer_test = true;
static const char *TAG = "BootButton";


static void IRAM_ATTR button_isr_handler(void *arg) {
    int button_pressed = 1;
    xQueueSendFromISR(button_queue, &button_pressed, NULL);
}

// Buton durumu kontrol eden task
void monitor_boot_button_task(void *arg) {
    int button_pressed;
    int current_index = 0;
    while (1) {
        if (xQueueReceive(button_queue, &button_pressed, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Button Pressed!");
    
            //to test
            /*
            uint8_t last_therapy_data[2];
            last_therapy_data[0] = 0x45;
            last_therapy_data[1] = 0x74;
            add_and_send_notification_info(HELMET_ON);
            //send_aperiodic_info(get_notification_handle(), last_therapy_data, sizeof(last_therapy_data));
            //
            if(timer_test)
            {
                if(current_index == 0) {
                    ESP_LOGI(TAG, "Start therapy timer!");

                    uint8_t last_therapy_data[10];
                    //get_last_therapy_data(last_therapy_data);

                    //start_new_therapy(20);
                }
                else if(current_index == 1) {
                    ESP_LOGI(TAG, "Stop therapy timer!");
                    //stop_therapy_timer();
                }

                current_index = (current_index + 1) % 2;
            }
            */
        }
    }
}

void initialize_boot_button_gpio() {
    gpio_config_t io_conf_button = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf_button);

    button_queue = xQueueCreate(10, sizeof(int));
    if (button_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return;
    }

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);

    /*
    if(timer_test)
    {
        ESP_LOGI(TAG, "Start inactivity timer!");
        start_inactivity_timer();
    }
    */
}
