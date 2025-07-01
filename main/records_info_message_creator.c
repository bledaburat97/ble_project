#include "records_info_message_creator.h"

#include "log_types.h"
#include "esp_log.h"
#include "ble_control.h"
#include "message_queue_manager.h"
#include "therapy_counter.h"
#include "matching_message_encoder.h"
#include "timer_state_info_message_creator.h"
#include "log_writer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RecordsInfoMessageCreator";

void send_records_info_message(uint16_t therapy_id) {
    read_and_set_records(therapy_id);
    ESP_LOGI(TAG, "therapy_id: %d, fragment_count: %d", therapy_id, get_fragment_count());

    for (uint16_t i = 0; i <= get_fragment_count(); i++) {
        const uint8_t* frag = get_fragment(i);
        size_t len = get_fragment_length(i);
        ESP_LOGI(TAG, "fragment id: %d, length: %d", i, len);

        if (frag != NULL && len > 0) {
            ESP_LOGI(TAG, "Sending fragment for therapy_id: %d, fragment_index: %d", therapy_id, i);
            ESP_LOG_BUFFER_HEX(TAG, frag, len);
            send_info_message(RECORDS_INFO_MESSAGE, (uint8_t*)frag, len,  0);
            vTaskDelay(pdMS_TO_TICKS(200));  //TODO Gerekirse bu süre MTU'ya göre ayarlanabilir
        }
    }
}

static void on_active_or_paused_therapy_existed()
{
    send_records_info_message(read_therapy_count() - 1);
}

void init_records_info_message_creator() {
    register_active_or_paused_therapy_info(on_active_or_paused_therapy_existed);
}


