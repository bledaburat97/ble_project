#include "device_event_router.h"

#include "device_manager.h"

#include "../helper/binary_message_parser.h"
#include "../button/main_button_controller.h"

#include "../storage/log_writer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DeviceEventRouter";

static QueueHandle_t s_event_queue;

void post_device_event(const DeviceEvent *event)
{
    if (!s_event_queue) return;

    if (xQueueSend(s_event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event type=%d", (int)event->type);
    }
}

void post_device_start_event() {
    DeviceEvent event = {0};
    event.type = EVT_DEVICE_START;
    post_device_event(&event);
}

void post_uncompleted_therapy_set_event(UncompletedTherapyInfo uncompletedTherapyInfo) {
    DeviceEvent event = {0};
    event.type = EVT_UNCOMPLETED_THERAPY_SET;
    event.data.uncompleted = uncompletedTherapyInfo;

    post_device_event(&event);
}

void post_button_press_event(ButtonPressType press_type)
{
    DeviceEvent event = {0};

    if (press_type == SHORT) {
        event.type = EVT_SHORT_BUTTON_PRESS;
    } else if (press_type == LONG) {
        event.type = EVT_LONG_BUTTON_PRESS;
    } else {
        return;
    }

    post_device_event(&event);
}

void post_timer_completed_event(NotificationType notification_type)
{
    DeviceEvent event = {0};

    if (notification_type == NOTIF_INACTIVITY_TIMER_EXPIRED) {
        event.type = EVT_INACTIVITY_TIMER_COMPLETED;
    } else if (notification_type == NOTIF_ALERT_TIMER_EXPIRED) {
        event.type = EVT_ALERT_TIMER_COMPLETED;
    } else if (notification_type == NOTIF_THERAPY_COMPLETED) {
        event.type = EVT_THERAPY_TIMER_COMPLETED;
    } else {
        return;
    }

    post_device_event(&event);
}

void post_temperature_alert_event(uint8_t sensor_index)
{
    DeviceEvent event = {0};
    event.type = EVT_TEMPERATURE_ALERT;
    event.data.temp_alert.sensor_index = sensor_index;
    post_device_event(&event);
}

void post_helmet_state_change_event(bool helmet_on)
{
    DeviceEvent event = {0};
    event.type = helmet_on ? EVT_HELMET_ON : EVT_HELMET_OFF;
    post_device_event(&event);
}

void post_therapy_state_change_request_event(const uint8_t *buffer, size_t length)
{
    (void)length;

    StatusChangeMessage status_change_message;
    if (!decode_status_change_message_bin(buffer, &status_change_message)) {
        ESP_LOGE(TAG, "StatusChangeMessage decode failed");
        return;
    }

    DeviceEvent event = {0};

    if (status_change_message.type == PAUSE) {
        event.type = EVT_PAUSE_REQUEST;
    } else if (status_change_message.type == STOP) {
        event.type = EVT_STOP_REQUEST;
    } else if (status_change_message.type == CONTINUE) {
        event.type = EVT_CONTINUE_REQUEST;
    } else {
        return;
    }

    post_device_event(&event);
}

void post_activation_request_event(const uint8_t *buffer, size_t length)
{
    (void)length;

    ActivationMessage activation_message;
    if (!decode_activation_message_bin(buffer, &activation_message)) {
        ESP_LOGE(TAG, "ActivationMessage decode failed");
        return;
    }

    DeviceEvent event = {0};
    event.type = EVT_ACTIVATION_REQUEST;
    event.data.activation.duration_s = activation_message.duration;
    memcpy(event.data.activation.brightness, activation_message.brightness, sizeof(event.data.activation.brightness));
    event.data.activation.brightness_present = true;
    

    post_device_event(&event);
}

static void event_router_task(void *arg)
{
    (void)arg;

    DeviceEvent event;

    while (1) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            handle_device_event(&event);
        }
    }
}

void init_device_event_router(void)
{
    s_event_queue = xQueueCreate(16, sizeof(DeviceEvent));
    if (!s_event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }
    xTaskCreate(event_router_task, "event_router_task", 4096, NULL, 10, NULL);
}