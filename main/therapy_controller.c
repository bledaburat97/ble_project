#include "therapy_controller.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "stdint.h"

static bool is_helmet_on;
static bool is_temperature_normal;
static bool is_lasers_running;
static bool is_therapy_active;

static SemaphoreHandle_t helmet_status_mutex = NULL;
static SemaphoreHandle_t temperature_status_mutex = NULL;
static SemaphoreHandle_t lasers_status_mutex = NULL;

static const char *Therapy_Controller_TAG = "TherapyController";


static void init_helmet_status_mutex() {
    helmet_status_mutex = xSemaphoreCreateMutex();
    if (helmet_status_mutex == NULL) {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to create helmet status mutex");
    }
}

static void init_temperature_status_mutex() {
    temperature_status_mutex = xSemaphoreCreateMutex();
    if (temperature_status_mutex == NULL) {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to create temperature status mutex");
    }
}

static void init_lasers_status_mutex() {
    lasers_status_mutex = xSemaphoreCreateMutex();
    if (lasers_status_mutex == NULL) {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to create lasers status mutex");
    }
}

bool can_therapy_start() {
    /*TODO
    if(!get_helmet_status() || !get_temperature_status()) {
        return false;
    }
        */
    return true;
}


void init_device_param_status(){
    init_helmet_status_mutex();
    init_temperature_status_mutex();
    init_lasers_status_mutex();
    set_helmet_status(false);
    set_temperature_status(true);
    set_lasers_status(false);
    set_therapy_status(false);
}


void set_helmet_status(bool status) {
    if (xSemaphoreTake(helmet_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(Therapy_Controller_TAG, "Setting helmet status to %s", status ? "ON" : "OFF");
        is_helmet_on = status;
        xSemaphoreGive(helmet_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to set helmet status");
    }
}

bool get_helmet_status() {
    bool status = false;
    if (xSemaphoreTake(helmet_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = is_helmet_on;
        xSemaphoreGive(helmet_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to get helmet status");
    }
    return status;
}

void set_temperature_status(bool status) {
    if (xSemaphoreTake(temperature_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(Therapy_Controller_TAG, "Setting temperature status to %s", status ? "ON" : "OFF");
        is_temperature_normal = status;
        xSemaphoreGive(temperature_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to set temperature status");
    }
}

bool get_temperature_status() {
    bool status = false;
    if (xSemaphoreTake(temperature_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = is_temperature_normal;
        ESP_LOGI(Therapy_Controller_TAG, "Retrieved temperature status: %s", status ? "ON" : "OFF");
        xSemaphoreGive(temperature_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to get temperature status");
    }
    return status;
}

void set_lasers_status(bool status) {
    if (xSemaphoreTake(lasers_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGI(Therapy_Controller_TAG, "Setting lasers status to %s", status ? "ON" : "OFF");
        is_lasers_running = status;
        xSemaphoreGive(lasers_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to set lasers status");
    }
}

bool get_lasers_status() {
    bool status = false;
    if (xSemaphoreTake(lasers_status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = is_lasers_running;
        ESP_LOGI(Therapy_Controller_TAG, "Retrieved lasers status: %s", status ? "ON" : "OFF");
        xSemaphoreGive(lasers_status_mutex);
    } else {
        ESP_LOGE(Therapy_Controller_TAG, "Failed to acquire mutex to get lasers status");
    }
    return status;
}

void set_therapy_status(bool status) {
    ESP_LOGI(Therapy_Controller_TAG, "Setting therapy status to %s", status ? "ON" : "OFF");
    is_therapy_active = status;
}

bool get_therapy_status() {
    ESP_LOGI(Therapy_Controller_TAG, "Retrieved therapy status: %s", is_therapy_active ? "ON" : "OFF");
    return is_therapy_active;
}