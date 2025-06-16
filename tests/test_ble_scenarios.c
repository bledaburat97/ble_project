#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Minimal Unity subset
#define TEST_ASSERT_EQUAL_INT(expected, actual) if((expected)!=(actual)) { printf("Assertion failed: %d != %d\n", (expected),(actual)); return 1; }

typedef enum {
    STATE_TEMPERATURE_ALERT,
    STATE_HUMIDITY_ALERT,
    STATE_ACTIVE,
    STATE_INACTIVITY,
    STATE_START,
    STATE_IDLE
} DeviceState;

typedef enum {
    THERAPY_STARTED_BY_APP,
    REGIONS_BRIGHTNESS_UPDATED
} NotificationType;

typedef struct {
    uint16_t therapy_duration;
    uint8_t brightness[6];
} ActivationMessage;

static DeviceState current_state;
static bool helmet_on = true;
static NotificationType last_notification;

DeviceState get_device_state() { return current_state; }
bool get_helmet_state() { return helmet_on; }
void start_new_therapy(uint16_t duration) { (void)duration; }
void set_brightness_of_region(int region, uint8_t level) { (void)region; (void)level; }
void add_and_send_notification_info(NotificationType type) { last_notification = type; }

static void handle_activation_message(const ActivationMessage* msg) {
    for(int i=0;i<6;i++) {
        set_brightness_of_region(i+1, msg->brightness[i]);
    }

    if(msg->therapy_duration > 0) {
        if(get_device_state() == STATE_INACTIVITY || get_device_state() == STATE_ACTIVE) {
            if(get_helmet_state()) {
                start_new_therapy(msg->therapy_duration);
                add_and_send_notification_info(THERAPY_STARTED_BY_APP);
            }
        }
    } else if(get_device_state() == STATE_ACTIVE) {
        add_and_send_notification_info(REGIONS_BRIGHTNESS_UPDATED);
    }
}

int main() {
    ActivationMessage msg = {0};
    for(int i=0;i<6;i++) msg.brightness[i] = i;

    // Scenario: brightness changed during active therapy
    current_state = STATE_ACTIVE;
    msg.therapy_duration = 0;
    last_notification = 0;
    handle_activation_message(&msg);
    TEST_ASSERT_EQUAL_INT(REGIONS_BRIGHTNESS_UPDATED, last_notification);

    // Scenario: start new therapy from inactivity
    current_state = STATE_INACTIVITY;
    msg.therapy_duration = 100;
    last_notification = 0;
    handle_activation_message(&msg);
    TEST_ASSERT_EQUAL_INT(THERAPY_STARTED_BY_APP, last_notification);
    printf("All tests passed.\n");
    return 0;
}
