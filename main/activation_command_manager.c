#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <esp_log.h>
#include <stddef.h>
#include "activation_command_manager.h"

static const char *TAG = "ActivationCommandManager";


bool decode_activation_command(const uint8_t *data, size_t len, ActivationCommand *activation_command) {
    /*
    if (len != ACTIVATION_COMMAND_BYTE_COUNT)
    { 
        ESP_LOGE(TAG, "Invalid data length: %d, expected 7", len);
        return false;
    }
    */
    ESP_LOGI(TAG, "Parse activation info");

    activation_command->received_command = data[0] & 0x01;
    activation_command->therapy_id = 20;
    activation_command->therapy_duration = (data[1] << 8) | data[2];
    activation_command->region_brightness[0] = data[3];
    activation_command->region_brightness[1] = data[4];
    activation_command->region_brightness[2] = 0;
    activation_command->region_brightness[3] = 25;
    activation_command->region_brightness[4] = 20;
    activation_command->region_brightness[5] = 80;

/*
    // Therapy ID (İlk 2 byte)
    activation_command->therapy_id = (data[0] << 8) | data[1];

    // Received Command (Bit 0)
    activation_command->received_command = (data[2] >> 7) & 0x01;

    // Therapy Duration (Bit 1-9) (9 bit)
    activation_command->therapy_duration = ((data[2] & 0x7F) << 4) | ((data[3] >> 4) & 0x0F);

    activation_command->region_brightness[0] = (data[3] & 0x0F) | ((data[4] >> 7) & 0x01);
    activation_command->region_brightness[1] = (data[4] >> 2) & 0x1F;
    activation_command->region_brightness[2] = ((data[4] & 0x03) << 3) | ((data[5] >> 5) & 0x07);
    activation_command->region_brightness[3] = data[5] & 0x1F;
    activation_command->region_brightness[4] =  (data[6] >> 3) & 0x1f;
    activation_command->region_brightness[5] = ((data[6] & 0x07) << 2) | ((data[7] >> 6) & 0x03);
*/
    // Debug logları
    ESP_LOGI(TAG, "Parsed Activation Info:");
    ESP_LOGI(TAG, "Therapy ID: %d", activation_command->therapy_id);
    ESP_LOGI(TAG, "Received Command: %d", activation_command->received_command);
    ESP_LOGI(TAG, "Therapy Duration: %d", activation_command->therapy_duration);
    
    for (int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "Region %d Brightness: %d", i, activation_command->region_brightness[i]);
    }
    return true;
}