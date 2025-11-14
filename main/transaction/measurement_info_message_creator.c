#include "measurement_info_message_creator.h"

#include "message_queue_manager.h"

#include "../helper/binary_message_encoder.h"

#include "../storage/log_types.h"
#include "../storage/log_writer.h"

#include "../i2c/temperature/temperature_sensor_controller.h"
#include "../i2c/humidity/humidity_sensor_controller.h"

#include "../state/timer_manager.h"
#include "../state/current_therapy_info_manager.h"

#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"

static const char *TAG = "MeasurementInfoMessageCreator";

static void add_and_send_measurement_info(uint8_t temperature, uint8_t humidity) {
    uint8_t data[] = {temperature, humidity};
    uint16_t passed_seconds = get_session_passed_seconds();
    esp_err_t err = add_log(MEASUREMENT_CHANGED, data, sizeof(data), passed_seconds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist measurement log: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "Measurement log recorded with temperature: %u, humidity: %u, passed_seconds: %u", temperature, humidity, passed_seconds);

    restart_duration_update_watchdog_timer();

    MeasurementInfoMessage message = {
        .temperature = temperature,
        .humidity = humidity,
        .passed_seconds = passed_seconds
    };

    ESP_LOGI(TAG, "Sent Temperature: %u", message.temperature);

    uint8_t buf[MEASUREMENT_INFO_SIZE];
    size_t len = encode_measurement_info_message_binary(&message, buf);
    send_info_message_to_queue(MEASUREMENT_INFO_MESSAGE, buf, len);
}

static void on_device_info_feedback_callback() {
    uint8_t current_temperature = measure_and_get_temperature();
    uint8_t current_humidity = measure_and_get_humidity();
    ESP_LOGI(TAG, "On device info feedback, Sending temperature: %u", current_temperature);
    add_and_send_measurement_info(current_temperature, current_humidity);
}

static void on_temperature_update(uint8_t temperature) {
    //ESP_LOGI(TAG, "On temperature update of temperature: %u.", temperature);
    add_and_send_measurement_info(temperature, get_humidity());
}

static void on_humidity_update(uint8_t humidity) {
    //ESP_LOGI(TAG, "On temperature update of temperature: %u.", temperature);
    add_and_send_measurement_info(get_temperature(), humidity);
}

void init_measurement_info_message_creator() {
    register_device_info_feedback_callback(on_device_info_feedback_callback);
    register_temperature_update(on_temperature_update);
    register_humidity_update(on_humidity_update);
}