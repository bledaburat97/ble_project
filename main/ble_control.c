#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_mac.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "sdkconfig.h"

#include "i2c_control.h"
#include "laser_driver_control.h"
#include "temperature_sensor_control.h"
#include "proximity_sensor_control.h"
#include "timer_management.h"
#include "json_parser.h"

#include "ble_control.h"



#define PROFILE_NUM 1
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static const char *BLE_TAG = "BLEControl";
static bool isBleConnected = false;
static bool isNotificationOn = true;
static uint8_t adv_config_done = 0;

SemaphoreHandle_t ble_mutex = NULL; // Define and initialize the mutex


static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND, //TODO:niye ADV_TYPE_NONCONN_IND değil?
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
	
	.include_name = true,
	.flag = ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
	.appearance = 384, //indicates generic remote control, it can be 0 which indicated unknown.
};

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       //Not get the gatt_if, so initial is ESP_GATT_IF_NONE
    },
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0) {
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;

    // Handle advertising start completion
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_TAG, "Advertising start failed");
        } else {
            ESP_LOGI(BLE_TAG, "Advertising started successfully");
        }
        break;

    // Handle advertising stop completion
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(BLE_TAG, "Advertising stopped successfully");
        }
        break;

    // Handle connection parameter update
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(BLE_TAG, "Connection parameters updated: status=%d, min_int=%d, max_int=%d, conn_int=%d, latency=%d, timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    // Handle packet length update
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(BLE_TAG, "Packet length updated: rx_len=%d, tx_len=%d, status=%d",
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len,
                 param->pkt_data_length_cmpl.status);
        break;

    // Add other GAP events if required
    default:
        ESP_LOGW(BLE_TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    //If event is register event, store the gatts_if for the profile
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if; // Save GATT interface for the profile
            esp_ble_gap_set_device_name("My BLE Device");
            esp_ble_gap_config_adv_data(&adv_data);

            ESP_LOGI(BLE_TAG, "GATTS_REG_EVT: Profile registered, app_id=%04x", param->reg.app_id);
        } else {
            ESP_LOGE(BLE_TAG, "GATTS_REG_EVT: Registration failed, app_id=%04x, status=%d",
                     param->reg.app_id, param->reg.status);
            return;
        }
    }

    // Dispatch events to the appropriate profile callback
    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) {
                gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }
}

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(BLE_TAG, "GATT profile registered, app_id: %d", param->reg.app_id);
        esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID,
        }, 8);  // Number of handles
        break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(BLE_TAG, "Service created, handle: %d", param->create.service_handle);
        // Add Laser Control Characteristic
        esp_ble_gatts_add_char(param->create.service_handle, &(esp_bt_uuid_t){
            .len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_LASER_CONTROL},
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ,
            NULL, NULL);

        // Add Proximity Characteristic
        esp_ble_gatts_add_char(param->create.service_handle, &(esp_bt_uuid_t){
            .len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_PROXIMITY},
            ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            NULL, NULL);

        // Add Temperature Characteristic
        esp_ble_gatts_add_char(param->create.service_handle, &(esp_bt_uuid_t){
            .len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_TEMPERATURE},
            ESP_GATT_PERM_READ,
            ESP_GATT_CHAR_PROP_BIT_READ,
            NULL, NULL);

        isBleConnected = true;
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(BLE_TAG, "ESP_GATTS_WRITE_EVT, handle: %d", param->write.handle);

        // Check if this write is for the Laser Control Characteristic
        if (param->write.handle == THERAPY_ACTIVATION_CHAR_HANDLE) {
            // Validate input data
            if (param->write.len == 0) {
                ESP_LOGE(BLE_TAG, "No data received for Laser Control");
                return;
            }

            // Parse incoming data
            char *data_copy = (char *)malloc(param->write.len + 1);
            if (data_copy == NULL) {
                ESP_LOGE(BLE_TAG, "Memory allocation failed");
                return;
            }
            memcpy(data_copy, param->write.value, param->write.len);
            data_copy[param->write.len] = '\0';
            
            TherapyActivationInfo *therapy_activation_info = parse_therapy_activation_info(data_copy);
            free(data_copy);
            
            if (therapy_activation_info == NULL) {
                ESP_LOGE(BLE_TAG, "Failed to parse therapy activation info");
                return;
            }

            ESP_LOGI(BLE_TAG, "Parsed Therapy ID: %u, Duration: %lu, Regions: %d",
                    therapy_activation_info->therapy_id,
                    therapy_activation_info->therapy_duration,
                    therapy_activation_info->num_of_changed_regions);

            //TODO:Check isHelmetOn
            //TODO: Check temperature sensors

            start_therapy_timer(therapy_activation_info->therapy_duration);

            if (therapy_activation_info->region_infos != NULL && therapy_activation_info->num_of_changed_regions > 0) {
                set_brightness(therapy_activation_info->region_infos, therapy_activation_info->num_of_changed_regions);
                ESP_LOGI(BLE_TAG, "Brightness updated successfully.");
            }

            else
            {
                ESP_LOGW(BLE_TAG, "No regions to update.");
            }

            free(therapy_activation_info->region_infos);
            free(therapy_activation_info);
            

        } else if (param->write.handle == PROXIMITY_CHAR_HANDLE) {
            
        } else if (param->write.handle == TEMPERATURE_CHAR_HANDLE) {

        } 
        break;

    case ESP_GATTS_READ_EVT: 
        if (param->read.handle == PROXIMITY_CHAR_HANDLE) {
            //uint8_t prox_data = read_proximity_sensor(); // Read proximity data
            //send_gatt_response(gatts_if, param, prox_data);
        } else if (param->read.handle == TEMPERATURE_CHAR_HANDLE) {
            //uint8_t temp_data = read_temperature_sensor(); // Read temperature data
            //send_gatt_response(gatts_if, param, temp_data);
        }
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(BLE_TAG, "Device disconnected, restarting advertising...");
        isBleConnected = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;

    default:
        break;
    }
}

void init_ble(){

    ble_mutex = xSemaphoreCreateMutex();
    if (!ble_mutex) {
        ESP_LOGE(BLE_TAG, "Failed to create BLE mutex");
        return;
    }
    
    esp_err_t ret;

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID); // Register laser and sensor service
    ESP_ERROR_CHECK(ret);

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(BLE_TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }
}

static void ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length)
{
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if, // GATT interface
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,  // Connection ID
        char_handle,                    // Characteristic handle
        data_length,                          // Data length
        data,                                  // Pointer to the data
        false                                      // Need confirmation?
    );

    if (ret != ESP_OK) {
        ESP_LOGE(BLE_TAG, "Failed to send: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(BLE_TAG, "Successfully sent.");
    }
}

void ble_notify_task(void *param) {
    while (isNotificationOn) {
        if (isBleConnected) {
            if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                // Send Laser status notification
                uint8_t led_data[2];
                setDataOfActiveLaserCount(led_data);
                ble_send_message(LASER_CTRL_CHAR_HANDLE, led_data, sizeof(led_data));

                // Send temperature notification
                uint8_t temp_data[6];
                get_temperature_of_all_sensors(temp_data);
                ble_send_message(TEMPERATURE_CHAR_HANDLE, temp_data, sizeof(temp_data));

                // Send helmet status notification
                uint8_t helmet_data = get_helmet_status() ? 0x01 : 0x00;
                ble_send_message(PROXIMITY_CHAR_HANDLE, &helmet_data, sizeof(helmet_data));

                // Release the mutex for other tasks
                xSemaphoreGive(ble_mutex);
            } else {
                ESP_LOGW(BLE_TAG, "Failed to obtain BLE mutex within 1 second");
            }
        } else {
            ESP_LOGW(BLE_TAG, "No active BLE connection, skipping notifications");
        }

        // Delay for a specified period (e.g., every 1 second)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void send_aperiodic_info(uint16_t char_handle, uint8_t* data)
{
    if (isBleConnected) {
        if (xSemaphoreTake(ble_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ble_send_message(char_handle, data, sizeof(data));
            xSemaphoreGive(ble_mutex);
        }
    }
    else {
        ESP_LOGW(BLE_TAG, "No active BLE connection, cannot send alert.");
    }
}
