#include "measurement_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "message_encoder.h"
#include "storage/log_writer.h"
#include "message_queue_manager.h"
#include "temperature_sensor_control.h"
#include "therapy_message_counter.h"
#include <string.h>
#include "message_queue_manager.h"
#include "timer_management.h"
#include "current_therapy_info_manager.h"

static const char *TAG = "MeasurementInfoMessageCreator";

static void add_and_send_measurement_info(uint8_t temperature) {
    uint8_t humidity = 0; //URGENT
    uint8_t data[] = {temperature, humidity}; //URGENT eğer temp veya hum değişmişse.
    uint16_t passed_seconds = get_session_passed_seconds();
    add_log(MEASUREMENT_CHANGED, data, sizeof(data), passed_seconds);
    ESP_LOGE(TAG, "Log of measurement with passed_seconds: %u", passed_seconds);

    restart_duration_update_watchdog_timer();

    MeasurementInfoMessage message;
    message.temperature = temperature;
    ESP_LOGI(TAG, "Sent Temperature: %u", message.temperature);
    message.humidity = humidity;
    message.passed_seconds = passed_seconds;

    bool isMessageJson = false;

    if(isMessageJson) {
        char *json_str = encode_measurement_info_message(&message);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "JSON encode failed");
            return;
        }

        size_t len = strlen(json_str);
        send_info_message_to_queue(MEASUREMENT_INFO_MESSAGE, (uint8_t*)json_str, len);

        free(json_str);
    }
    else {
        uint8_t buf[MEASUREMENT_INFO_SIZE];
        size_t len = encode_measurement_info_message_binary(&message, buf);
        send_info_message_to_queue(MEASUREMENT_INFO_MESSAGE, buf, len);
    }
}

static void on_device_info_feedback_callback() {
    uint8_t current_temperature = 0; //URGENT temperature'ı ölç.
    add_and_send_measurement_info(current_temperature);
}

static void on_temperature_update(uint8_t temperature) {
    //ESP_LOGI(TAG, "On temperature update of temperature: %u.", temperature);
    add_and_send_measurement_info(temperature);
}

void init_measurement_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
    register_temperature_update(on_temperature_update);
}