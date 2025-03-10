#include "proximity_sensor_control.h"
#include "i2c_control.h"
#include "esp_log.h"
#include "proximity_sensor_config.h"
#include "lp_core_queue_manager.h"

#define DEFAULT_LED_CURRENT 20
#define DEFAULT_INTERRUPT_CONTROL_BIT_COUNT 2

static const char *TAG = "ProximitySensorControl";
static bool detection_status[2] = {false, false};
static uint8_t master_num_of_sensors[2] = {I2C_FIRST_MASTER_NUM, I2C_SECOND_MASTER_NUM};

uint16_t lowerThreshold = 1792;
uint16_t higherThreshold = 2304;

static void add_lp_read_command_to_queue(uint8_t device_address, uint8_t reg_address) {
    uint32_t lp_core_device_address = 0x00000000 | (device_address & 0xFF);
    uint32_t lp_core_register =  0x00000000 | (reg_address & 0xFF);
    uint32_t lp_core_byte_count = 1;
    uint32_t lp_core_command = 2;
    uint32_t lp_core_value = 0;

    ESP_LOGI(TAG, "Queue'ya read ekle: Command=%lu, Register=%lu, Value=%lu, Device Address=%lu, Byte count=%lu", 
        lp_core_command, lp_core_register, lp_core_value, lp_core_device_address, lp_core_byte_count);

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

static void enable_periodic_self_measurement(uint8_t i2c_master_num)
{
    uint8_t config_byte = 0x00;
    
    config_byte = ENABLE_SELF_TIMED_MODE(config_byte);
    ESP_LOGI(TAG, "Self-Timed Mode Enabled.");

    config_byte = ENABLE_PERIODIC_MEASUREMENT(config_byte);
    ESP_LOGI(TAG, "Periodic Measurement Enabled.");

    if(i2c_master_num == I2C_FIRST_MASTER_NUM){
        write_register(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte, 1, i2c_master_num);
    }
    else{
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte);
    }

}


static void set_proximity_measurement_rate(uint8_t i2c_master_num, ProximityRate rate)
{
    ProximityRateRegister proximityRateConfig;
    proximityRateConfig.proximity_rate = rate;
    uint8_t config_byte = *(uint8_t*)&proximityRateConfig;
    if(i2c_master_num == I2C_FIRST_MASTER_NUM){
        write_register(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG, &config_byte, 1, i2c_master_num);
    }
    else{
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG, &config_byte);
    }
}

// INFRARED LED CURRENT

static void set_led_current(uint8_t i2c_master_num, uint8_t current)
{
    if (current % 10 != 0 || current > 200) {
        ESP_LOGE(TAG, "Invalid current value: %u", current);
        current = DEFAULT_LED_CURRENT;
    }
    uint8_t current_value_bits = current / 10;
    IRLedCurrentRegister ir_led_current_config;
    ir_led_current_config.ir_led_current = current_value_bits;
    uint8_t config_byte = *(uint8_t*)&ir_led_current_config;
    if(i2c_master_num == I2C_FIRST_MASTER_NUM){
        write_register(VCNL_3020_ADDRESS, IR_LED_CURRENT_REG, &config_byte, 1, i2c_master_num);
    }
    else{
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

    return int_bits;
}

static void set_interrupt_control(uint8_t i2c_master_num, uint8_t interrupt_control_count) {
    uint8_t int_count_exceed_bits = get_interrupt_count_exceed_bits(interrupt_control_count);

    InterruptControlRegister int_control_config;
    int_control_config.int_thres_sel = 0;
    int_control_config.int_thres_en = 1;
    int_control_config.int_prox_ready_en = 0;
    int_control_config.reserved1 = 0;
    int_control_config.reserved2 = 0;
    int_control_config.int_count_exceed = int_count_exceed_bits;
    uint8_t config_byte = *(uint8_t*)&int_control_config;
    ESP_LOGE(TAG, "config_byt2: %u", config_byte);

    if(i2c_master_num == I2C_FIRST_MASTER_NUM) {
        write_register(VCNL_3020_ADDRESS, INTERRUPT_CONTROL_REG, &config_byte, 1, i2c_master_num);
    }
    else{
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

static void set_high_threshold(uint8_t i2c_master_num, uint16_t high_threshold)
{
    uint8_t high_byte, low_byte;
    split_into_bytes(high_threshold, &high_byte, &low_byte);
    if(i2c_master_num == I2C_FIRST_MASTER_NUM) {
        write_register(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_HIGH, &high_byte, 1, i2c_master_num);
        write_register(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_LOW, &low_byte, 1, i2c_master_num);
    }
    else {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_HIGH, &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, HIGH_THRESHOLD_REG_LOW, &low_byte);
    }
}

static void set_low_threshold(uint8_t i2c_master_num, uint16_t low_threshold)
{
    uint8_t high_byte, low_byte;
    split_into_bytes(low_threshold, &high_byte, &low_byte);
    if(i2c_master_num == I2C_FIRST_MASTER_NUM) {
        write_register(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_HIGH, &high_byte, 1, i2c_master_num);
        write_register(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_LOW, &low_byte, 1, i2c_master_num);
    }
    else {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_HIGH, &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, LOW_THRESHOLD_REG_LOW, &low_byte);
    }
}

static bool get_sensor_detection_status(uint8_t sensor_index)
{
    return detection_status[sensor_index];
}

static void set_sensor_detection_status(uint8_t sensor_index, bool status) {
    detection_status[sensor_index] = status;
}

static void set_default_thresholds(uint8_t sensor_index) {
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    set_high_threshold(i2c_master_num, higherThreshold);
    set_low_threshold(i2c_master_num, lowerThreshold);
}

static void increase_thresholds(uint8_t sensor_index) {
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    set_high_threshold(i2c_master_num, 0xFFFF);
    set_low_threshold(i2c_master_num, higherThreshold - 100);
}

static void reset_interrupt(uint8_t sensor_index, ProximityThresholdType type) {
    uint8_t byte;
    if(type == HIGH) { byte = 0x01;}
    else if(type == LOW) { byte = 0x02;}
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    if(i2c_master_num == I2C_FIRST_MASTER_NUM) {
        write_register(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &byte, 1, i2c_master_num);
    }
    else {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &byte);
    }
}

static bool check_other_sensor_detected(uint8_t asserted_sensor_index)
{
    uint8_t other_sensor_index = asserted_sensor_index == 0 ? asserted_sensor_index + 1 : asserted_sensor_index - 1;
    return get_sensor_detection_status(other_sensor_index);
}

static void read_proximity_of_sensor(uint8_t i2c_master_num) {
    uint8_t highProximityByte;
    if (i2c_master_num == I2C_FIRST_MASTER_NUM){
        read_register(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_HIGH, &highProximityByte, 1, i2c_master_num);
        ESP_LOGI(TAG, "highProximityByte: %u", highProximityByte);
        uint8_t lowProximityByte;
        read_register(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_LOW, &lowProximityByte, 1, i2c_master_num);
        ESP_LOGI(TAG, "highProximityByte: %u", lowProximityByte);
    }
    else {
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_HIGH);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RESULT_REG_LOW);
    }
}

void log_proximity(uint8_t sensor_index) {
    ESP_LOGI(TAG, "Log proximity.");
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    read_proximity_of_sensor(i2c_master_num);
}

void initialize_proximity_sensors()
{
    //enable_periodic_self_measurement(master_num_of_sensors[0]);
    enable_periodic_self_measurement(master_num_of_sensors[1]);

    //set_proximity_measurement_rate(master_num_of_sensors[0], PROX_RATE_31_25);
    set_proximity_measurement_rate(master_num_of_sensors[1], PROX_RATE_31_25);

    //set_led_current(master_num_of_sensors[0], DEFAULT_LED_CURRENT);
    set_led_current(master_num_of_sensors[1], DEFAULT_LED_CURRENT);

    //set_interrupt_control(master_num_of_sensors[0], DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);
    set_interrupt_control(master_num_of_sensors[1], DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);

    //set_high_threshold(master_num_of_sensors[0], higherThreshold);
    set_high_threshold(master_num_of_sensors[1], higherThreshold);

    //set_low_threshold(master_num_of_sensors[0], lowerThreshold);
    set_low_threshold(master_num_of_sensors[1], lowerThreshold);

    //set_sensor_detection_status(0, false);
    set_sensor_detection_status(1, false);
}

void request_excess_status(uint8_t asserted_sensor_index)
{
    uint8_t i2c_master_num = master_num_of_sensors[asserted_sensor_index];
    if (i2c_master_num == I2C_FIRST_MASTER_NUM){
        uint8_t status_of_sensor;
        read_register(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG, &status_of_sensor, 1, i2c_master_num);
        check_interrupt_status(status_of_sensor, false);
    }
    else
    {
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG);
    }
}

void check_interrupt_status(uint8_t status, bool is_lp)
{
    uint8_t asserted_sensor_index = is_lp ? 1 : 0;
    bool isExcessDetected = false;
    ESP_LOGI(TAG, "check_interrupt_status.");

    if((status & 0x01) != 0)
    {
        ESP_LOGE(TAG, "high is exceeded.");

        isExcessDetected = true;
        //high geçilmiş.
        if(!get_sensor_detection_status(asserted_sensor_index)){
            set_sensor_detection_status(asserted_sensor_index, true);
            increase_thresholds(asserted_sensor_index);
            reset_interrupt(asserted_sensor_index, HIGH);
            if(check_other_sensor_detected(asserted_sensor_index)) {
                //TODO: set_helmet_status(true);
                ESP_LOGI(TAG, "HELMET_ON.");
                //TODO: send_notification(HELMET_ON);
            }
        }
        else {
            reset_interrupt(asserted_sensor_index, HIGH);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES.");
            //TODO: send_notification(WRONG_THRESHOLD_VALUES);
        }
    }

    if((status & 0x02) != 0) 
    {
        //low geçilmiş
        if(isExcessDetected)
        {
            ESP_LOGE(TAG, "BOTH HIGH AND LOW THRESHOLDS EXCEEDED.");
            reset_interrupt(asserted_sensor_index, LOW);
            return;
        }
        ESP_LOGE(TAG, "low is exceeded.");

        isExcessDetected = true;
        if(get_sensor_detection_status(asserted_sensor_index)){
            set_sensor_detection_status(asserted_sensor_index, false);
            set_default_thresholds(asserted_sensor_index);
            reset_interrupt(asserted_sensor_index, LOW);
            //TODO: set_helmet_status(false);
            ESP_LOGI(TAG, "HELMET_OFF.");
            //TODO: send_notification(HELMET_OFF);
        }
        else{
            reset_interrupt(asserted_sensor_index, LOW);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES.");
            //TODO: send_notification(WRONG_THRESHOLD_VALUES);
        }
    }

    if(!isExcessDetected && get_sensor_detection_status(asserted_sensor_index)){
        //ESP_LOGE(TAG, "TRY TO START LASERS AGAIN.");
        //yanlış int tespitinden dolayı kapatılan lazerler açılmalı.
        //TODO: TRY TO START LASERS AGAIN
    }

}