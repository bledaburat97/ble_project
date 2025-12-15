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

#define DEFAULT_LED_CURRENT                20
#define DEFAULT_INTERRUPT_CONTROL_BIT_COUNT 2

static const char *TAG = "ProximitySensorController";

static bool s_is_hp_prox_sensor = false;
static bool s_is_lp_prox_sensor = false;
static bool s_hp_prox_sensor_detection_status = false;
static bool s_lp_prox_sensor_detection_status = false;

/**
 * LP-core için "read register" komutunu kuyruğa ekler.
 */
static void add_lp_read_command_to_queue(uint8_t device_address, uint8_t reg_address)
{
    uint32_t lp_core_device_address = (uint32_t)(device_address & 0xFFU);
    uint32_t lp_core_register       = (uint32_t)(reg_address & 0xFFU);
    uint32_t lp_core_byte_count     = 1U;
    uint32_t lp_core_command        = 2U;
    uint32_t lp_core_value          = 0U;

    ESP_LOGI(TAG,
             "LP read queued: cmd=%lu, reg=0x%02lX, value=%lu, addr=0x%02lX, bytes=%lu",
             lp_core_command,
             lp_core_register,
             lp_core_value,
             lp_core_device_address,
             lp_core_byte_count);

    queue_add_task(lp_core_command,
                   lp_core_register,
                   lp_core_value,
                   lp_core_device_address,
                   lp_core_byte_count);
}

/**
 * LP-core için "write register" komutunu kuyruğa ekler.
 * Şu an tek byte yazımı destekliyor.
 */
static void add_lp_write_command_to_queue(uint8_t device_address,
                                          uint8_t reg_address,
                                          uint8_t *data)
{
    uint32_t lp_core_device_address = (uint32_t)(device_address & 0xFFU);
    uint32_t lp_core_register       = (uint32_t)(reg_address & 0xFFU);
    uint32_t lp_core_byte_count     = 1U;
    uint32_t lp_core_command        = 1U;
    uint32_t lp_core_value          = (uint32_t)(data[0] & 0xFFU);

    ESP_LOGI(TAG,
             "LP write queued: cmd=%lu, reg=0x%02lX, value=0x%02lX, addr=0x%02lX, bytes=%lu",
             lp_core_command,
             lp_core_register,
             lp_core_value,
             lp_core_device_address,
             lp_core_byte_count);

    queue_add_task(lp_core_command,
                   lp_core_register,
                   lp_core_value,
                   lp_core_device_address,
                   lp_core_byte_count);
}

/**
 * VCNL3020'yi self-timed + periodic measurement moduna alır.
 * HP sensör için doğrudan I2C, LP sensör için LP-core kuyruk üzerinden çalışır.
 */
static void enable_periodic_self_measurement(void)
{
    uint8_t config_byte = 0x00;

    config_byte = ENABLE_SELF_TIMED_MODE(config_byte);
    ESP_LOGI(TAG, "Self-timed mode enabled.");

    config_byte = ENABLE_PERIODIC_MEASUREMENT(config_byte);
    ESP_LOGI(TAG, "Periodic measurement enabled.");

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       COMMAND_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG);
    }
}

static void disable_periodicness(void)
{
    uint8_t config_byte = 0x01;

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       COMMAND_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG);
    }
}

static void disable_selftimed(void)
{
    uint8_t config_byte = 0x00;

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       COMMAND_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG, &config_byte);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, COMMAND_REG);
    }
}

/**
 * Proximity ölçüm oranını (Hz) ayarlar.
 */
static void set_proximity_measurement_rate(ProximityRate rate)
{
    ProximityRateRegister proximity_rate_config;
    proximity_rate_config.proximity_rate = rate;

    uint8_t config_byte = *(uint8_t *)&proximity_rate_config;

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       PROXIMITY_RATE_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG, &config_byte);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, PROXIMITY_RATE_REG);
    }
}

/**
 * IR LED akımını 10 mA adımlar ile ayarlar (0–200 mA).
 * Geçersiz değer gelirse DEFAULT_LED_CURRENT kullanılır.
 */
static void set_led_current(uint8_t current)
{
    if ((current % 10U) != 0U || current > 200U) {
        ESP_LOGE(TAG, "Invalid IR LED current value: %u, using default=%d",
                 current, DEFAULT_LED_CURRENT);
        current = DEFAULT_LED_CURRENT;
    }

    uint8_t current_value_bits = (uint8_t)(current / 10U);

    IRLedCurrentRegister ir_led_current_config;
    ir_led_current_config.ir_led_current = current_value_bits;

    uint8_t config_byte = *(uint8_t *)&ir_led_current_config;

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       IR_LED_CURRENT_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      IR_LED_CURRENT_REG,
                                      &config_byte);
    }
}

/**
 * Interrupt tetiklenmesi için kaç ardışık ölçüm (2^n) gerektiğini belirleyen bit sayısını döndürür.
 * Girilen count 1,2,4,8,...,128 şeklinde bir power-of-two olmalıdır.
 * Geçersiz ise DEFAULT_INTERRUPT_CONTROL_BIT_COUNT kullanılır.
 */
static uint8_t get_interrupt_count_exceed_bits(uint8_t count)
{
    if (count == 0U || (count & (count - 1U)) != 0U || count > 128U) {
        count = DEFAULT_INTERRUPT_CONTROL_BIT_COUNT;
    }

    uint8_t int_bits = 0;
    while (count > 1U) {
        count >>= 1U;
        int_bits++;
    }

    ESP_LOGI(TAG, "Interrupt control bit count: %u", int_bits);
    return int_bits;
}

/**
 * Interrupt control register'ını konfigüre eder.
 * - threshold interrupt açık
 * - ready interrupt kapalı
 * - int_count_exceed, get_interrupt_count_exceed_bits ile hesaplanır.
 */
static void set_interrupt_control(uint8_t interrupt_control_count)
{
    uint8_t int_count_exceed_bits = get_interrupt_count_exceed_bits(interrupt_control_count);

    InterruptControlRegister int_control_config;
    int_control_config.int_thres_sel      = 0;
    int_control_config.int_thres_en       = 1;
    int_control_config.int_prox_ready_en  = 0;
    int_control_config.reserved1          = 0;
    int_control_config.reserved2          = 0;
    int_control_config.int_count_exceed   = int_count_exceed_bits;

    uint8_t config_byte = *(uint8_t *)&int_control_config;

    if (s_is_hp_prox_sensor) {
        write_register(VCNL_3020_ADDRESS,
                       INTERRUPT_CONTROL_REG,
                       &config_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    }

    if (s_is_lp_prox_sensor) {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      INTERRUPT_CONTROL_REG,
                                      &config_byte);
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_CONTROL_REG);
    }
}

/**
 * 16-bit değeri iki byte'a böler.
 */
static void split_into_bytes(uint16_t input, uint8_t *high_byte, uint8_t *low_byte)
{
    if (high_byte == NULL || low_byte == NULL) {
        return;
    }

    *high_byte = (uint8_t)((input >> 8) & 0xFFU);
    *low_byte  = (uint8_t)(input & 0xFFU);
}

/**
 * High threshold register'larını ayarlar.
 */
static void set_high_threshold(uint16_t high_threshold, bool is_lp)
{
    uint8_t high_byte = 0;
    uint8_t low_byte  = 0;

    split_into_bytes(high_threshold, &high_byte, &low_byte);

    if (!is_lp) {
        write_register(VCNL_3020_ADDRESS,
                       HIGH_THRESHOLD_REG_HIGH,
                       &high_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
        write_register(VCNL_3020_ADDRESS,
                       HIGH_THRESHOLD_REG_LOW,
                       &low_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    } else {
        ESP_LOGI(TAG,
                 "LP high threshold set: high=0x%02X, low=0x%02X",
                 high_byte, low_byte);

        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      HIGH_THRESHOLD_REG_HIGH,
                                      &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      HIGH_THRESHOLD_REG_LOW,
                                      &low_byte);
    }
}

/**
 * Low threshold register'larını ayarlar.
 */
static void set_low_threshold(uint16_t low_threshold, bool is_lp)
{
    uint8_t high_byte = 0;
    uint8_t low_byte  = 0;

    split_into_bytes(low_threshold, &high_byte, &low_byte);

    if (!is_lp) {
        write_register(VCNL_3020_ADDRESS,
                       LOW_THRESHOLD_REG_HIGH,
                       &high_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
        write_register(VCNL_3020_ADDRESS,
                       LOW_THRESHOLD_REG_LOW,
                       &low_byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    } else {
        ESP_LOGI(TAG,
                 "LP low threshold set: high=0x%02X, low=0x%02X",
                 high_byte, low_byte);

        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      LOW_THRESHOLD_REG_HIGH,
                                      &high_byte);
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      LOW_THRESHOLD_REG_LOW,
                                      &low_byte);
    }
}

static bool get_sensor_detection_status(bool is_lp)
{
    return is_lp ? s_lp_prox_sensor_detection_status
                 : s_hp_prox_sensor_detection_status;
}

static void set_sensor_detection_status(bool is_lp, bool status)
{
    if (!is_lp) {
        s_hp_prox_sensor_detection_status = status;
        ESP_LOGI(TAG,
                 "HP proximity sensor detection status: %s",
                 status ? "true" : "false");
    } else {
        s_lp_prox_sensor_detection_status = status;
        ESP_LOGI(TAG,
                 "LP proximity sensor detection status: %s",
                 status ? "true" : "false");
    }
}

/**
 * Proximity sensörleri için normal (helmet off) threshold setini yazar.
 */
static void set_default_thresholds(bool is_lp)
{
    ESP_LOGI(TAG, "Set default thresholds for proximity sensor (is_lp=%d).", is_lp);
    set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, is_lp);
    set_low_threshold(PROXIMITY_LOWER_THRESHOLD, is_lp);
}

/**
 * "Helmet on" sonrası threshold’ları daha geniş banda taşıyarak
 * histerezis sağlayan threshold setini yazar.
 */
static void increase_thresholds(bool is_lp)
{
    ESP_LOGI(TAG, "Increase thresholds for proximity sensor (is_lp=%d).", is_lp);
    set_high_threshold(0xFFFFU, is_lp);
    set_low_threshold(PROXIMITY_MAX_LOWER_THRESHOLD, is_lp);
}

/**
 * Interrupt status register'ında ilgili bitleri sıfırlayarak
 * threshold interrupt'ını clear eder.
 */
static void reset_interrupt(bool is_lp, ProximityThresholdType type)
{
    ESP_LOGI(TAG, "Reset interrupt of proximity sensor (is_lp=%d, type=%d).",
             is_lp, type);

    uint8_t byte = 0;
    if (type == HIGH) {
        byte = 0x01U;
    } else if (type == LOW) {
        byte = 0x02U;
    }

    if (!is_lp) {
        write_register(VCNL_3020_ADDRESS,
                       INTERRUPT_STATUS_REG,
                       &byte,
                       1,
                       I2C_FIRST_MASTER_NUM);
    } else {
        add_lp_write_command_to_queue(VCNL_3020_ADDRESS,
                                      INTERRUPT_STATUS_REG,
                                      &byte);
    }
}

/**
 * HP sensör için anlık proximity değerini okur,
 * LP sensör için LP-core tarafına read komutu gönderir.
 * (Bu fonksiyon şu an test amaçlı "yakınlık okuma" mantığı içeriyor.)
 */
void read_proximity_of_sensors(void)
{
    if (s_is_hp_prox_sensor) {
        uint8_t high_proximity_byte = 0;

        read_register(VCNL_3020_ADDRESS,
                      PROXIMITY_RESULT_REG_HIGH,
                      &high_proximity_byte,
                      1,
                      I2C_FIRST_MASTER_NUM);

        if (high_proximity_byte > 10U) {
            change_helmet_state(true);
            ESP_LOGI(TAG, "HP proximity above threshold (value=%u)", high_proximity_byte);
            // add_and_send_notification_info(NOTIF_HELMET_ON); // test için
        } else {
            ESP_LOGI(TAG, "HP proximity below threshold (value=%u)", high_proximity_byte);
            change_helmet_state(false); // TODO: prod'da aç
        }

        uint8_t low_proximity_byte = 0;
        read_register(VCNL_3020_ADDRESS,
                      PROXIMITY_RESULT_REG_LOW,
                      &low_proximity_byte,
                      1,
                      I2C_FIRST_MASTER_NUM);
        (void)low_proximity_byte;
    }

    if (s_is_lp_prox_sensor) {
        if (is_all_config_written()) {
            add_lp_read_command_to_queue(VCNL_3020_ADDRESS,
                                         PROXIMITY_RESULT_REG_HIGH);
            add_lp_read_command_to_queue(VCNL_3020_ADDRESS,
                                         PROXIMITY_RESULT_REG_LOW);
        }
    }
}

/**
 * Periyodik olarak proximity ölçümü yapan task (debug / test amaçlı).
 */
void proximity_read_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        ESP_LOGI(TAG, "Reading proximity values from sensors...");
        read_proximity_of_sensors();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * HP ve/veya LP proximity sensörlerini başlatır:
 * - Ölçüm oranı
 * - IR LED akımı
 * - Interrupt control
 * - Periodic measure
 * - Initial thresholds
 */
void initialize_proximity_sensors(bool hp_prox_sensor_exist, bool lp_prox_sensor_exist)
{
    s_is_hp_prox_sensor = hp_prox_sensor_exist;
    s_is_lp_prox_sensor = lp_prox_sensor_exist;

    vTaskDelay(pdMS_TO_TICKS(100));

    disable_periodicness();
    vTaskDelay(pdMS_TO_TICKS(10));
    disable_selftimed();
    vTaskDelay(pdMS_TO_TICKS(10));
    set_proximity_measurement_rate(PROX_RATE_31_25);
    set_led_current(DEFAULT_LED_CURRENT);
    set_interrupt_control(DEFAULT_INTERRUPT_CONTROL_BIT_COUNT);
    enable_periodic_self_measurement();

    if (s_is_hp_prox_sensor) {
        ESP_LOGI(TAG, "HP proximity thresholds are set.");
        set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, false);
        set_low_threshold(PROXIMITY_LOWER_THRESHOLD, false);
    }

    if (s_is_lp_prox_sensor) {
        ESP_LOGI(TAG, "LP proximity thresholds are set.");
        set_high_threshold(PROXIMITY_HIGHER_THRESHOLD, true);
        set_low_threshold(PROXIMITY_LOWER_THRESHOLD, true);
    }

    set_sensor_detection_status(false, false);
    set_sensor_detection_status(true, false);
}

/**
 * INT hattı tetiklendiğinde, ilgili sensörün INTERRUPT_STATUS_REG değerini
 * okur veya LP-core'a okuma komutu gönderir.
 */
void request_excess_status(bool is_lp)
{
    if (!is_lp) {
        uint8_t status_of_sensor = 0;
        read_register(VCNL_3020_ADDRESS,
                      INTERRUPT_STATUS_REG,
                      &status_of_sensor,
                      1,
                      I2C_FIRST_MASTER_NUM);
        check_interrupt_status(status_of_sensor, false);
    } else {
        ESP_LOGI(TAG, "LP interrupt status read queued.");
        add_lp_read_command_to_queue(VCNL_3020_ADDRESS, INTERRUPT_STATUS_REG);
    }
}

/**
 * INTERRUPT_STATUS_REG değeri yorumlanarak:
 * - High threshold (helmet yakın)
 * - Low threshold (helmet uzak)
 * durumları belirlenir.
 * İki sensör de birlikte değerlendirilerek:
 * - Helmet on/off
 * - Wrong measurement
 * gibi notification'lar üretilebilir.
 */
void check_interrupt_status(uint8_t status, bool is_lp)
{
    ESP_LOGI(TAG, "Check interrupt status (is_lp=%d, status=0x%02X)", is_lp, status);

    bool is_excess_detected = false;

    // High threshold exceeded
    if ((status & 0x01U) != 0U) {
        ESP_LOGI(TAG, "High threshold exceeded (is_lp=%d).", is_lp);

        is_excess_detected = true;

        if (!get_sensor_detection_status(is_lp)) {
            set_sensor_detection_status(is_lp, true);
            increase_thresholds(is_lp);
            ESP_LOGI(TAG, "Reset high-threshold interrupt.");
            reset_interrupt(is_lp, HIGH);

            if (get_sensor_detection_status(!is_lp)) {
                ESP_LOGI(TAG, "Both proximity sensors detected object (HELMET_ON).");
                change_helmet_state(true); // TODO: prod'da aç
            }
        } else {
            reset_interrupt(is_lp, HIGH);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES (high already active).");
            add_and_send_notification_info(WRONG_PROX_MEASUREMENT);
        }
    }

    // Low threshold exceeded
    if ((status & 0x02U) != 0U) {
        if (is_excess_detected) {
            ESP_LOGE(TAG, "Both HIGH and LOW thresholds exceeded simultaneously.");
            reset_interrupt(is_lp, LOW);
            return;
        }

        ESP_LOGI(TAG, "Low threshold exceeded (is_lp=%d).", is_lp);
        is_excess_detected = true;

        if (get_sensor_detection_status(is_lp)) {
            set_sensor_detection_status(is_lp, false);
            set_default_thresholds(is_lp);
            reset_interrupt(is_lp, LOW);
            ESP_LOGI(TAG, "HELMET_OFF detected (is_lp=%d).", is_lp);
            change_helmet_state(false); // TODO: prod'da aç
        } else {
            reset_interrupt(is_lp, LOW);
            ESP_LOGE(TAG, "WRONG_THRESHOLD_VALUES (low while detection was false).");
            add_and_send_notification_info(WRONG_PROX_MEASUREMENT);
        }
    }

    if (!is_excess_detected && get_sensor_detection_status(is_lp)) {
        ESP_LOGE(TAG, "TRY TO START LASERS AGAIN (spurious interrupt, detection still true).");
        // TODO: yanlış INT tespiti nedeniyle kapatılan lazerler yeniden başlatılabilir.
    }
}
