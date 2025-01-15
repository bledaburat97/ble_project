#include "proximity_sensor_control.h"
#include "i2c_control.h"
#include "esp_log.h"


#define VCNL_3030_ADDRESS 0x13
#define REG0 0x80
#define REG2 0x82
#define REG3 0x83
#define REG9 0x89
#define REG10 0x8A
#define REG11 0x8B
#define REG12 0x8C
#define REG13 0x8D
#define REG14 0x8E

#define DEFAULT_LED_CURRENT 20
#define DEFAULT_INTERRUPT_CONTROL_BIT_COUNT 4
#define DEFAULT_MEASUREMENT_RATE_INDEX 2

static const char *Proximity_TAG = "ProximitySensorControl";
static bool detection_status[2] = {false, false};
static uint8_t master_num_of_sensors[2] = {I2C_FIRST_MASTER_NUM, I2C_SECOND_MASTER_NUM};

uint16_t lowerThreshold = 5300;
uint16_t higherThreshold = 5500;
static bool helmet_status;

//MEASUREMENT RATE

const MeasurementRate vcnl3020_measurement_rates[] = {
    {0b000, 1.95},        // 1.95 measurements/s (DEFAULT)
    {0b001, 3.90625},     // 3.90625 measurements/s
    {0b010, 7.8125},      // 7.8125 measurements/s
    {0b011, 16.625},      // 16.625 measurements/s
    {0b100, 31.25},       // 31.25 measurements/s
    {0b101, 62.5},        // 62.5 measurements/s
    {0b110, 125.0},       // 125 measurements/s
    {0b111, 250.0}        // 250 measurements/s
};

static uint8_t get_measurement_rate_bits(float measurement_rate) {
    for (int i = 0; i < sizeof(vcnl3020_measurement_rates) / sizeof(vcnl3020_measurement_rates[0]); i++) {
        if (vcnl3020_measurement_rates[i].measurement_rate == measurement_rate) {
            return vcnl3020_measurement_rates[i].rw_bits;
        }
    }
    return vcnl3020_measurement_rates[DEFAULT_MEASUREMENT_RATE_INDEX].rw_bits;
}

static void set_proximity_measurement_rate(uint8_t i2c_master_num, float measurement_rate)
{
    uint8_t measurement_rate_bits = get_measurement_rate_bits(measurement_rate);
    write_register(VCNL_3030_ADDRESS, REG2, &measurement_rate_bits, 1, i2c_master_num);
}


// INFRARED LED CURRENT

static void set_led_current(uint8_t i2c_master_num, uint8_t current)
{
    if (current % 10 != 0) {
        ESP_LOGE(Proximity_TAG, "Invalid current value: %u", current);
        current = DEFAULT_LED_CURRENT;
    }
    uint8_t current_value_bits = current / 10;
    write_register(VCNL_3030_ADDRESS, REG3, &current_value_bits, 1, i2c_master_num);
}


// INTERRUPT CONTROL COUNT

static uint8_t get_interrupt_control_bits(uint8_t count)
{
    if (count == 0 || (count & (count - 1)) != 0 || count > 128) {
        count = DEFAULT_INTERRUPT_CONTROL_BIT_COUNT;
    }

    // Calculate the most significant 3 bits based on the exponential relationship
    uint8_t int_bits = 0;
    while (count > 1) {
        count >>= 1;
        int_bits++;
    }

    return int_bits;
}

static void set_interrupt_control(uint8_t i2c_master_num, uint8_t interrupt_control_count) {
    uint8_t int_bits = get_interrupt_control_bits(interrupt_control_count);

    //Shift int_bits to the most significant 3 bits and set the second bit(int_thres_en) to 1
    uint8_t result_byte = (int_bits << 5) | (1 << 1);

    write_register(VCNL_3030_ADDRESS, REG9, &result_byte, 1, i2c_master_num);
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
    write_register(VCNL_3030_ADDRESS, REG12, &high_byte, 1, i2c_master_num);
    write_register(VCNL_3030_ADDRESS, REG13, &low_byte, 1, i2c_master_num);
}

static void set_low_threshold(uint8_t i2c_master_num, uint16_t low_threshold)
{
    uint8_t high_byte, low_byte;
    split_into_bytes(low_threshold, &high_byte, &low_byte);
    write_register(VCNL_3030_ADDRESS, REG10, &high_byte, 1, i2c_master_num);
    write_register(VCNL_3030_ADDRESS, REG11, &low_byte, 1, i2c_master_num);
}


//PERIDIC SELF MEASUREMENT STATUS

static void enable_periodic_self_measurement(uint8_t i2c_master_num) {
    uint8_t byte = 0x03;
    write_register(VCNL_3030_ADDRESS, REG0, &byte, 1, i2c_master_num);
}

bool check_threshold_exceeded(uint8_t sensor_index, bool is_high) {
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    uint8_t status_of_sensor;
    read_register(VCNL_3030_ADDRESS, REG14, &status_of_sensor, 1, i2c_master_num);

    if (is_high) {
        return ((status_of_sensor & 0x02) == 0) && ((status_of_sensor & 0x01) != 0);
    } else {
        return ((status_of_sensor & 0x02) != 0) && ((status_of_sensor & 0x01) == 0);
    }
}

bool get_sensor_detection_status(uint8_t sensor_index)
{
    return detection_status[sensor_index];
}

void set_sensor_detection_status(uint8_t sensor_index, bool status) {
    detection_status[sensor_index] = status;
}

void set_default_thresholds(uint8_t sensor_index) {
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    set_high_threshold(i2c_master_num, higherThreshold);
    set_low_threshold(i2c_master_num, lowerThreshold);
}

void increase_thresholds(uint8_t sensor_index) {
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    set_high_threshold(i2c_master_num, 0xFFFF);
    set_low_threshold(i2c_master_num, higherThreshold - 100);
}

void reset_interrupt(uint8_t sensor_index) {
    uint8_t byte_to_make_int_high = 0x01;
    uint8_t i2c_master_num = master_num_of_sensors[sensor_index];
    write_register(VCNL_3030_ADDRESS, REG14, &byte_to_make_int_high, 1, i2c_master_num);
}

bool check_other_sensor_detected(uint8_t asserted_sensor_index)
{
    uint8_t other_sensor_index = asserted_sensor_index == 0 ? asserted_sensor_index + 1 : asserted_sensor_index - 1;
    return get_sensor_detection_status(other_sensor_index);
}

void initialize_proximity_sensors()
{
    set_proximity_measurement_rate(master_num_of_sensors[0], 7.8125);
    set_proximity_measurement_rate(master_num_of_sensors[1], 7.8125);

    set_led_current(master_num_of_sensors[0], DEFAULT_LED_CURRENT);
    set_led_current(master_num_of_sensors[1], DEFAULT_LED_CURRENT);

    set_interrupt_control(master_num_of_sensors[0], DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);
    set_interrupt_control(master_num_of_sensors[1], DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);

    set_high_threshold(master_num_of_sensors[0], higherThreshold);
    set_high_threshold(master_num_of_sensors[1], higherThreshold);

    set_low_threshold(master_num_of_sensors[0], lowerThreshold);
    set_low_threshold(master_num_of_sensors[1], lowerThreshold);

    enable_periodic_self_measurement(master_num_of_sensors[0]);
    enable_periodic_self_measurement(master_num_of_sensors[1]);

    set_sensor_detection_status(0, false);
    set_sensor_detection_status(1, false);
}