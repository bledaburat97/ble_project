#include "measurement_info_message_creator.h"

#include "storage/log_types.h"
#include "esp_log.h"
#include "json_encoder.h"
#include "storage/log_writer.h"
#include "message_queue_manager.h"
#include "temperature_sensor_control.h"
#include "therapy_message_counter.h"
#include <string.h>
#include "message_queue_manager.h"
#include "timer_management.h"

static const char *TAG = "MeasurementInfoMessageCreator";

static void add_and_send_measurement_info(uint8_t temperature) {
    uint8_t humidity = 0; //URGENT
    uint8_t data[] = {temperature, humidity}; //URGENT eğer temp veya hum değişmişse.
    uint16_t passed_seconds = get_passed_duration();
    add_log(MEASUREMENT_CHANGED, data, sizeof(data), passed_seconds);

    MeasurementInfoMessage message;
    message.temperature = temperature;
    message.humidity = humidity;
    message.passed_seconds = passed_seconds;
    message.message_id = get_message_id();

    char *json_str = encode_measurement_info_message(&message);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "JSON encode failed");
        return;
    }

    size_t len = strlen(json_str);
    send_info_message_to_queue(MEASUREMENT_INFO_MESSAGE, (uint8_t*)json_str, len, message.message_id);

    free(json_str);
}

static void on_device_info_feedback_callback() {
    uint8_t current_temperature = 0; //URGENT temperature'ı ölç.
    add_and_send_measurement_info(current_temperature);
}

static void on_temperature_update(uint8_t temperature) {
    add_and_send_measurement_info(temperature);
}

void init_measurement_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
    register_temperature_update(on_temperature_update);
}