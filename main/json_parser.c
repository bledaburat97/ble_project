
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
        cJSON *on = cJSON_GetObjectItem(region_status_info, "on");
        cJSON *brightness = cJSON_GetObjectItem(region_status_info, "brightness");

        if (!cJSON_IsNumber(region_id) || !cJSON_IsBool(on)) {
            ESP_LOGW(JsonTAG, "Invalid JSON in region: 'region_id' or 'on' field missing/invalid");
            continue;
        }

        therapy_activation_info->region_infos[index].region_id = (uint8_t)region_id->valueint;
        therapy_activation_info->region_infos[index].on = cJSON_IsTrue(on);
        therapy_activation_info->region_infos[index].brightness = (brightness && cJSON_IsNumber(brightness)) ? (uint8_t)brightness->valueint : 0;

        ESP_LOGI(JsonTAG, "Parsed region - RegionId: %d, On: %d, Brightness: %d",
                 therapy_activation_info->region_infos[index].region_id,
                 therapy_activation_info->region_infos[index].on,
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
