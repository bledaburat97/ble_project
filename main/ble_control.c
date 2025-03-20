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
#include "esp_bt.h"
#include "esp_mac.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "sdkconfig.h"

#include "ble_control.h"

#define PROFILE_NUM 1
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static const char *TAG = "BLEControl";
static uint8_t adv_config_done = 0;
static void (*on_connect_callback)() = NULL;
static void (*on_disconnect_callback)() = NULL;
static void (*on_write_activation_callback)(const uint8_t*, size_t) = NULL;

/*
static const uint8_t GATTS_SERVICE_UUID128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 //TODO: Unique UUID yarat.
};
*/

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND, //TODO:niye ADV_TYPE_NONCONN_IND değil?
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
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
    uint16_t notification_handle;
    uint16_t periodic_handle;
    uint16_t activation_handle;
    esp_bt_uuid_t char_uuid;
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
            ESP_LOGE(TAG, "Advertising start failed");
        } else {
            ESP_LOGI(TAG, "Advertising started successfully");
        }
        break;

    // Handle advertising stop completion
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising stop failed");
        } else {
            ESP_LOGI(TAG, "Advertising stopped successfully");
        }
        break;

    // Handle connection parameter update
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Connection parameters updated: status=%d, min_int=%d, max_int=%d, conn_int=%d, latency=%d, timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;

    // Handle packet length update
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(TAG, "Packet length updated: rx_len=%d, tx_len=%d, status=%d",
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len,
                 param->pkt_data_length_cmpl.status);
        break;

    // Add other GAP events if required
    default:
        ESP_LOGW(TAG, "Unhandled GAP event: %d", event);
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    //If event is register event, store the gatts_if for the profile
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if; // Save GATT interface for the profile
            esp_ble_gap_set_device_name("BLE-DA");
            esp_ble_gap_config_adv_data(&adv_data);

            ESP_LOGI(TAG, "GATTS_REG_EVT: Profile registered, app_id=%04x", param->reg.app_id);
        } else {
            ESP_LOGE(TAG, "GATTS_REG_EVT: Registration failed, app_id=%04x, status=%d",
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
        ESP_LOGI(TAG, "GATT profile registered, app_id: %d", param->reg.app_id);
        esp_err_t err = esp_ble_gatts_create_service(gatts_if, &(esp_gatt_srvc_id_t){
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = 0xFFF0
            //{GATTS_SERVICE_UUID128[0], GATTS_SERVICE_UUID128[1], GATTS_SERVICE_UUID128[2], GATTS_SERVICE_UUID128[3],
            //GATTS_SERVICE_UUID128[4], GATTS_SERVICE_UUID128[5], GATTS_SERVICE_UUID128[6], GATTS_SERVICE_UUID128[7],
            //GATTS_SERVICE_UUID128[8], GATTS_SERVICE_UUID128[9], GATTS_SERVICE_UUID128[10], GATTS_SERVICE_UUID128[11],
            //GATTS_SERVICE_UUID128[12], GATTS_SERVICE_UUID128[13], GATTS_SERVICE_UUID128[14], GATTS_SERVICE_UUID128[15]}
        }, 8); // Handle sayısı
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Service creation failed, error code: 0x%X", err);
        }
        break;

    case ESP_GATTS_ADD_CHAR_EVT:
    ESP_LOGI(TAG, "Characteristic added, handle: %d, UUID: 0x%04X",
        param->add_char.attr_handle, param->add_char.char_uuid.uuid.uuid16);

    if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_PERIODIC) {
        gl_profile_tab[PROFILE_A_APP_ID].periodic_handle = param->add_char.attr_handle;
        ESP_LOGI(TAG, "Periodic Characteristic Handle: %d", param->add_char.attr_handle);
    } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_NOTIFICATION) {
        gl_profile_tab[PROFILE_A_APP_ID].notification_handle = param->add_char.attr_handle;
        ESP_LOGI(TAG, "Notification Characteristic Handle: %d", param->add_char.attr_handle);
    } else if (param->add_char.char_uuid.uuid.uuid16 == GATTS_CHAR_UUID_ACTIVATION) {
        gl_profile_tab[PROFILE_A_APP_ID].activation_handle = param->add_char.attr_handle;
        ESP_LOGI(TAG, "Activation Characteristic Handle: %d", param->add_char.attr_handle);
    }
    break;

    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "Service created, status: %d, handle: %d", param->create.status, param->create.service_handle);
        if (param->create.status == ESP_GATT_OK) {
            gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(param->create.service_handle);
        } else {
            ESP_LOGE(TAG, "Service creation failed with status: %d", param->create.status);
        }        
        ESP_LOGI(TAG, "CREATE_SERVICE_EVT, status %d, service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
    
        esp_err_t add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_PERIODIC},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_ACTIVATION},
                               ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }

        add_char_ret =
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
            &(esp_bt_uuid_t){.len = ESP_UUID_LEN_16, .uuid.uuid16 = GATTS_CHAR_UUID_NOTIFICATION},
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                               NULL,
                               NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT, handle: %d", param->write.handle);

        if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].activation_handle) {
            if(param->write.value != NULL) {
                if (on_write_activation_callback) {
                    on_write_activation_callback(param->write.value, param->write.len);
                }
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);

            }
            else {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_PDU, NULL);
            }
        } 
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Device disconnected, restarting advertising...");
        if (on_disconnect_callback) {
            on_disconnect_callback();
        }
        esp_ble_gap_start_advertising(&adv_params);
        break;
    
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected");
        if (on_connect_callback) {
            on_connect_callback();
        }
        break;

    default:
        break;
    }
}

static esp_err_t ble_send_message(uint16_t char_handle, uint8_t* data, size_t data_length)
{
    esp_err_t ret = esp_ble_gatts_send_indicate(
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if,  // GATT interface
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,   // Connection ID
        char_handle,                                // Characteristic handle
        data_length,                                // Data length
        data,                                       // Pointer to the data
        true                                        // Need confirmation?
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Successfully sent.");
    }
    return ret;
}

esp_err_t send_periodic_data(uint8_t* data, size_t data_length)
{
    return ble_send_message(gl_profile_tab[PROFILE_A_APP_ID].periodic_handle, data, data_length);
}

esp_err_t send_notification_data(uint8_t* data, size_t data_length)
{
    return ble_send_message(gl_profile_tab[PROFILE_A_APP_ID].notification_handle, data, data_length);
}

void register_on_connect_callback(void (*callback)()) {
    on_connect_callback = callback;
}

void register_on_disconnect_callback(void (*callback)()) {
    on_disconnect_callback = callback;
}

void register_on_write_activation_callback(void (*callback)(const uint8_t*, size_t)) {
    on_write_activation_callback = callback;
}

esp_err_t start_registering_and_advertising()
{
    esp_err_t ret;

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_register_callback(gap_event_handler);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    ESP_ERROR_CHECK(ret);

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }
    esp_ble_gap_start_advertising(&adv_params);
    return ret;
}