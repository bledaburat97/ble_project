
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <esp_log.h>
#include <stddef.h>

#include "laser_driver_control.h"
#include "json_parser.h"

static const char *JsonTAG = "JsonParser";

TherapyActivationInfo* parse_therapy_activation_info(const char *json_data) {
    cJSON *json = cJSON_Parse(json_data);
    if (json == NULL) {
        ESP_LOGE(JsonTAG, "Failed to parse JSON");
        return NULL;
    }

    // Allocate memory for TherapyActivationInfo
    TherapyActivationInfo *therapy_activation_info = (TherapyActivationInfo *)malloc(sizeof(TherapyActivationInfo));
    if (therapy_activation_info == NULL) {
        ESP_LOGE(JsonTAG, "Failed to allocate memory for TherapyActivationInfo");
        cJSON_Delete(json);
        return NULL;
    }

    therapy_activation_info->region_infos = NULL;

    // Parse received_command
    cJSON *command = cJSON_GetObjectItem(json, "command");
    if (!cJSON_IsNumber(command)) {
        ESP_LOGE(JsonTAG, "Invalid or missing 'command'");
        free(therapy_activation_info);
        cJSON_Delete(json);
        return NULL;
    }
    therapy_activation_info->received_command = (uint8_t)command->valueint;


    // Parse therapyId
    cJSON *therapy_id = cJSON_GetObjectItem(json, "therapy_id");
    if (!cJSON_IsNumber(therapy_id)) {
        ESP_LOGE(JsonTAG, "Invalid or missing 'therapy_id'");
        goto cleanup;
    }
    therapy_activation_info->therapy_id = (uint16_t)therapy_id->valueint;

    // Parse duration
    cJSON *therapy_duration = cJSON_GetObjectItem(json, "therapy_duration");
    if (!cJSON_IsNumber(therapy_duration)) {
        ESP_LOGE(JsonTAG, "Invalid or missing 'therapy_duration'");
        goto cleanup;
    }
    therapy_activation_info->therapy_duration = (uint32_t)therapy_duration->valueint;

    // Parse changedRegions
    cJSON *region_infos = cJSON_GetObjectItem(json, "region_infos");
    if (!cJSON_IsArray(region_infos)) {
        ESP_LOGE(JsonTAG, "Invalid or missing 'region_infos'");
        goto cleanup;
    }

    int changed_regions_count = cJSON_GetArraySize(region_infos);
    therapy_activation_info->region_infos = (RegionStatusChangedInfo *)malloc(sizeof(RegionStatusChangedInfo) * changed_regions_count);
    if (therapy_activation_info->region_infos == NULL) {
        ESP_LOGE(JsonTAG, "Failed to allocate memory for region_infos");
        goto cleanup;
    }

    int index = 0;
    cJSON *region_status_info = NULL;
    cJSON_ArrayForEach(region_status_info, region_infos) {
        cJSON *region_id = cJSON_GetObjectItem(region_status_info, "region_id");
        cJSON *brightness = cJSON_GetObjectItem(region_status_info, "brightness");

        if (!cJSON_IsNumber(region_id)) {
            ESP_LOGW(JsonTAG, "Invalid JSON in region: 'region_id' field missing/invalid");
            continue;
        }

        therapy_activation_info->region_infos[index].region_id = (uint8_t)region_id->valueint;
        therapy_activation_info->region_infos[index].brightness = (brightness && cJSON_IsNumber(brightness)) ? (uint8_t)brightness->valueint : 0;

        ESP_LOGI(JsonTAG, "Parsed region - RegionId: %d, Brightness: %d",
                 therapy_activation_info->region_infos[index].region_id,
                 therapy_activation_info->region_infos[index].brightness);
        index++;
    }

    therapy_activation_info->num_of_changed_regions = index;

    cJSON_Delete(json);
    return therapy_activation_info;

    cleanup:
    if (therapy_activation_info) {
        if (therapy_activation_info->region_infos) {
            free(therapy_activation_info->region_infos);
        }
        free(therapy_activation_info);
    }
    cJSON_Delete(json);
    return NULL;
}


void parse_activation_info(const uint8_t *data, size_t len, ActivationInfo *activation_info) {
    if (len != 7) {  // Beklenen veri uzunluğu tam 7 byte olmalı
        ESP_LOGE(JsonTAG, "Invalid data length: %d, expected 7", len);
        return;
    }
    ESP_LOGI(JsonTAG, "Parse activation info");

    // Therapy ID (İlk 2 byte)
    activation_info->therapy_id = (data[0] << 8) | data[1];

    // Received Command (Bit 0)
    activation_info->received_command = (data[2] >> 7) & 0x01;

    // Therapy Duration (Bit 1-9) (9 bit)
    activation_info->therapy_duration = ((data[2] & 0x7F) << 2) | ((data[3] >> 6) & 0x03);

    // Region Brightness (30 bit - 6x5 bit)
    activation_info->region_brightness[0] = (data[3] >> 1) & 0x1F;
    activation_info->region_brightness[1] = ((data[3] & 0x01) << 4) | ((data[4] >> 4) & 0x0F);
    activation_info->region_brightness[2] = ((data[4] & 0x0F) << 1) | ((data[5] >> 7) & 0x01);
    activation_info->region_brightness[3] = (data[5] >> 2) & 0x1F;
    activation_info->region_brightness[4] = ((data[5] & 0x03) << 3) | ((data[6] >> 5) & 0x07);
    activation_info->region_brightness[5] = data[6] & 0x1F;

    // Debug logları
    ESP_LOGI(JsonTAG, "Parsed Activation Info:");
    ESP_LOGI(JsonTAG, "Therapy ID: %d", activation_info->therapy_id);
    ESP_LOGI(JsonTAG, "Received Command: %d", activation_info->received_command);
    ESP_LOGI(JsonTAG, "Therapy Duration: %d", activation_info->therapy_duration);
    
    for (int i = 0; i < 6; i++) {
        ESP_LOGI(JsonTAG, "Region %d Brightness: %d", i, activation_info->region_brightness[i]);
    }
}

void encode_periodic_info(const PeriodicInfo *info, uint8_t *output) {
    // Therapy ID (16-bit, ilk 2 byte)
    output[0] = (info->therapy_id >> 8) & 0xFF;
    output[1] = info->therapy_id & 0xFF;

    // Remaining Duration (9-bit) + Temperature (7-bit)
    output[2] = (info->remaining_duration >> 1) & 0xFF; // İlk 8 bit
    output[3] = ((info->remaining_duration & 0x01) << 7) | (info->temperature & 0x7F); // Son 1 bit + 7 bit Temperature

    // Humidity (5-bit) + Reserved (3-bit)
    output[4] = ((info->humidity & 0x1F) << 3) | (0x00); // Reserved bits are set to 0
}
