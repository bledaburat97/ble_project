#include "proximity_sensor_controller.h"

#include "proximity_sensor_config.h"

#include "../i2c_control.h"

#include "../../lp_core/lp_core_queue_manager.h"

#include "../../state/state_manager.h"
#include "../../state/general_manager.h"

#include "../../storage/log_types.h"

#include "../../transaction/incoming_message_handler.h"
#include "../../transaction/notification_info_message_creator.h"

#include "../../device_configuration.h"

#include "esp_log.h"

#define DEFAULT_LED_CURRENT 20
#define DEFAULT_INTERRUPT_CONTROL_BIT_COUNT 2

static bool is_hp_prox_sensor = false;
static bool is_lp_prox_sensor = false;
static bool hp_prox_sensor_detection_status = false;
static bool lp_prox_sensor_detection_status = false;

static const char *TAG = "ProximitySensorControl";

static void add_lp_read_command_to_queue(uint8_t device_address, uint8_t reg_address) {
    uint32_t lp_core_device_address = 0x00000000 | (device_address & 0xFF);
    uint32_t lp_core_register =  0x00000000 | (reg_address & 0xFF);
    uint32_t lp_core_byte_count = 1;
    uint32_t lp_core_command = 2;
    uint32_t lp_core_value = 0;

    //ESP_LOGI(TAG, "Queue'ya read ekle: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu", lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);

    queue_add_task(lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);
}

static void add_lp_write_command_to_queue(uint8_t device_address, uint8_t reg_address, uint8_t* data) {
    uint32_t lp_core_device_address = 0x00000000 | (device_address & 0xFF);
    uint32_t lp_core_register =  0x00000000 | (reg_address & 0xFF);
    uint32_t lp_core_byte_count = 1;
    uint32_t lp_core_command = 1;
    uint32_t lp_core_value = 0x00000000 | (data[0] & 0xFF);

    ESP_LOGI(TAG, "Queue'ya yaz: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu", 
        lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);

    queue_add_task(lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);
}

static void enable_periodic_self_measurement()
{
    uint8_t config_byte = 0x00;
    
    config_byte = ENABLE_SELF_TIMED_MODE(config_byte);
    ESP_LOGI(TAG, "Self-Timed Mode Enabled.");

    config_byte = ENABLE_PERIODIC_MEASUREMENT(config_byte);
    ESP_LOGI(TAG, "Periodic Measurement Enabled.");

    if (is_hp_prox_sensor)
    {
        write_register(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    if (is_lp_prox_sensor) 
    {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte);
    }
}

static void set_proximity_measurement_rate(ProximityRate rate)
{
    ProximityRateRegister proximityRateConfig;
    proximityRateConfig.proximity_rate = rate;
    uint8_t config_byte = *(uint8_t*)&proximityRateConfig;
    if (is_hp_prox_sensor)
    {
        write_register(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG, &config_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    if (is_lp_prox_sensor) 
    {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG, &config_byte);
    }
}

// INFRARED LED CURRENT

static void set_led_current(uint8_t current)
{
    if (current % 10 != 0 || current > 200) {
        ESP_LOGE(TAG, "Invalid current value: %u", current);
        current = DEFAULT_LED_CURRENT;
    }
    uint8_t current_value_bits = current / 10;
    IRLedCurrentRegister ir_led_current_config;
    ir_led_current_config.ir_led_current = current_value_bits;
    uint8_t config_byte = *(uint8_t*)&ir_led_current_config;
    if (is_hp_prox_sensor)
    {
        write_register(VCNL_3020_ADDRESS, IR_LED_CURRENT_REG, &config_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    if (is_lp_prox_sensor) 
    {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, IR_LED_CURRENT_REG, &config_byte);
    }
}

// INTERRUPT CONTROL COUNT

static uint8_t get_interrupt_count_exceed_bits(uint8_t count)
{
    if (count == 0 || (count & (count - 1)) != 0 || count > 128) {
        count = DEFAULT_INTERRUPT_CONTROL_BIT_COUNT;
    }
    uint8_t int_bits = 0;
    while (count > 1) {
        count >>= 1;
        int_bits++;
    }
    ESP_LOGI(TAG, "interrup control bit count: %u", int_bits);
    return int_bits;
}

static void set_interrupt_control(uint8_t interrupt_control_count) {
    uint8_t int_count_exceed_bits = get_interrupt_count_exceed_bits(interrupt_control_count);

    InterruptControlRegister int_control_config;
    int_control_config.int_thres_sel = 0;
    int_control_config.int_thres_en = 1;
    int_control_config.int_prox_ready_en = 0;
    int_control_config.reserved1 = 0;
    int_control_config.reserved2 = 0;
    int_control_config.int_count_exceed = int_count_exceed_bits;
    uint8_t config_byte = *(uint8_t*)&int_control_config;

    if (is_hp_prox_sensor)
    {
        write_register(VCNL_3020_ADDRESS, INTERRUPT_CONTROL_REG, &config_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    if (is_lp_prox_sensor) 
    {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_CONTROL_REG, &config_byte);
    }
}

// THRESHOLD

static void split_into_bytes(uint16_t input, uint8_t *high_byte, uint8_t *low_byte) {
    if (high_byte == NULL || low_byte == NULL) {
        return;
    }

    *high_byte = (input >> 8) & 0xFF;
    *low_byte = input & 0xFF;
}

static void set_high_threshold(uint16_t high_threshold, bool is_lp)
{
    uint8_t high_byte, low_byte;
    split_into_bytes(high_threshold, &high_byte, &low_byte);
    if (!is_lp)
    {
        write_register(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_HIGH, &high_byte, 1, I2C_FIRST_MASTER_NUM);
        write_register(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_LOW, &low_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    else
    {
        ESP_LOGI(TAG, "High threshold's high byte is set: %u", high_byte);
        ESP_LOGI(TAG, "High threshold's low byte is set: %u", low_byte);

        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_HIGH, &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_LOW, &low_byte);
    }
}

static void set_low_threshold(uint16_t low_threshold, bool is_lp)
{
    uint8_t high_byte, low_byte;
    split_into_bytes(low_threshold, &high_byte, &low_byte);
    if (!is_lp)
    {
        write_register(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_HIGH, &high_byte, 1, I2C_FIRST_MASTER_NUM);
        write_register(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_LOW, &low_byte, 1, I2C_FIRST_MASTER_NUM);
    }
    else
    {
        ESP_LOGI(TAG, "Low threshold's high byte is set: %u", high_byte);
        ESP_LOGI(TAG, "Low threshold's low byte is set: %u", low_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_HIGH, &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_LOW, &low_byte);
    }
}

static bool get_sensor_detection_status(bool is_lp)
{
    if(!is_lp) { return hp_prox_sensor_detection_status;} 
    return lp_prox_sensor_detection_status;
}

static void set_sensor_detection_status(bool is_lp, bool status) {
    if(!is_lp) {
        if(status) {
            ESP_LOGI(TAG, "HP proximity sensor detected");
        }
        else {
            ESP_LOGI(TAG, "HP proximity sensor not detected");
        }
        hp_prox_sensor_detection_status = status;
    } 
    else {
        if(status) {
            ESP_LOGI(TAG, "LP proximity sensor detected");
        }
        else {
            ESP_LOGI(TAG, "LP proximity sensor not detected");
        }
        lp_prox_sensor_detection_status = status;
    }
}

static void set_default_thresholds(bool is_lp) {
    ESP_LOGI(TAG, "Set default thresholds for proximity sensor.");
    set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, is_lp);
    set_low_threshold(PROXIMITY_LOWER_THRESHOLD, is_lp);
}

static void increase_thresholds(bool is_lp) {
    ESP_LOGI(TAG, "Increase thresholds for proximity sensor.");
    set_high_threshold(0xFFFF, is_lp);
    set_low_threshold(PROXIMITY_MAX_LOWER_THRESHOLD, is_lp);
}

static void reset_interrupt(bool is_lp, ProximityThresholdType type) {
    ESP_LOGI(TAG, "Reset interrupy of proximity sensor.");
    uint8_t byte;
    if(type == HIGH) { byte = 0x01;}
    else if(type == LOW) { byte = 0x02;}
    if(!is_lp)
    {
        write_register(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &byte, 1, I2C_FIRST_MASTER_NUM);
    }
    else {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &byte);
    }
}

void read_proximity_of_sensors() {
    //ESP_LOGI(TAG, "Log proximity.");

    if (is_hp_prox_sensor)
    {
        uint8_t highProximityByte;
        read_register(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_HIGH, &highProximityByte, 1, I2C_FIRST_MASTER_NUM);
        if(highProximityByte > 10) {
            change_helmet_state(true);
            ESP_LOGI(TAG, "Normal prox true");
            //add_and_send_notification_info(NOTIF_HELMET_ON); //for test
        }

        if(highProximityByte < 10) {
            ESP_LOGI(TAG, "Normal prox false");

            //change_helmet_state(false); //TODO open this in prod
            //add_and_send_notification_info(NOTIF_HELMET_ON); //for test
        }

        uint8_t lowProximityByte;
        read_register(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_LOW, &lowProximityByte, 1, I2C_FIRST_MASTER_NUM);

    }
    
    if (is_lp_prox_sensor)
    {
        if(is_all_config_written()) {
            add_lp_read_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_HIGH);
            add_lp_read_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_LOW);
        }
    }
}

void proximity_read_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Yakınlık sensöründen okuma yapılıyor...");
        read_proximity_of_sensors();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void initialize_proximity_sensors(bool hp_prox_sensor_exist, bool lp_prox_sensor_exist)
{
    is_hp_prox_sensor = hp_prox_sensor_exist;
    is_lp_prox_sensor = lp_prox_sensor_exist;

    enable_periodic_self_measurement();
    set_proximity_measurement_rate(PROX_RATE_31_25);
    set_led_current(DEFAULT_LED_CURRENT);
    set_interrupt_control(DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);

    if(is_hp_prox_sensor){
        ESP_LOGI(TAG, "Proximity Threshold is set.");
        set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, false);
        set_low_threshold(PROXIMITY_LOWER_THRESHOLD, false);
    }
    if(is_lp_prox_sensor){
        set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, true);
        set_low_threshold(PROXIMITY_LOWER_THRESHOLD, true);
    }

    set_sensor_detection_status(false, false);
    set_sensor_detection_status(true, false);
}

void request_excess_status(bool is_lp)
{
    if (!is_lp){
        uint8_t status_of_sensor;
        read_register(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &status_of_sensor, 1, I2C_FIRST_MASTER_NUM);
        check_interrupt_status(status_of_sensor, false);
    }
    else
    {
        ESP_LOGI(TAG, "ADD_LP_READ_COMMAND_TO_QUEUE");
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG);
    }
}

void check_interrupt_status(uint8_t status, bool is_lp)
{
    ESP_LOGI(TAG, "Check interrupt status, status: %u", status);
    bool isExcessDetected = false;

    if((status & 0x01) != 0)
    {
        ESP_LOGI(TAG, "High Threshold is exceeded.");

        isExcessDetected = true;
        //high geçilmiş.
        if(!get_sensor_detection_status(is_lp)){
            set_sensor_detection_status(is_lp, true);
            increase_thresholds(is_lp);
            ESP_LOGI(TAG, "Reset interrupt.");
            reset_interrupt(is_lp, HIGH);
            add_and_send_notification_info(NOTIF_HELMET_ON); //for test
            if(get_sensor_detection_status(!is_lp)) {
                ESP_LOGI(TAG, "Both prox true");
                change_helmet_state(true);
                ESP_LOGI(TAG, "HELMET_ON.");
            }
        }
        else {
            reset_interrupt(is_lp, HIGH);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES.");
            add_and_send_notification_info(WRONG_PROX_MEASUREMENT);
        }
    }

    if((status & 0x02) != 0) 
    {
        //low geçilmiş
        if(isExcessDetected)
        {
            ESP_LOGE(TAG, "BOTH HIGH AND LOW THRESHOLDS EXCEEDED.");
            reset_interrupt(is_lp, LOW);
            return;
        }
        ESP_LOGI(TAG, "Low Threshold is exceeded.");

        isExcessDetected = true;
        if(get_sensor_detection_status(is_lp)){
            set_sensor_detection_status(is_lp, false);
            set_default_thresholds(is_lp);
            reset_interrupt(is_lp, LOW);
            change_helmet_state(false);
            ESP_LOGI(TAG, "HELMET_OFF.");
            add_and_send_notification_info(NOTIF_HELMET_OFF); //for test
        }
        else{
            reset_interrupt(is_lp, LOW);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES.");
            add_and_send_notification_info(WRONG_PROX_MEASUREMENT);
        }
    }

    if(!isExcessDetected && get_sensor_detection_status(is_lp)){
        ESP_LOGE(TAG, "TRY TO START LASERS AGAIN.");
        //yanlış int tespitinden dolayı kapatılan lazerler açılmalı.
        //TODO: TRY TO START LASERS AGAIN
    }

}