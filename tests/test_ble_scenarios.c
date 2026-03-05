#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Minimal Unity subset
#define TEST_ASSERT_EQUAL_INT(expected, actual) if((expected)!=(actual)) { printf("Assertion failed: %d != %d\n", (expected),(actual)); return 1; }

#include "../main/transaction_logic.h"
#include "../main/laser_driver_control.h"
#include "../main/timer_management.h"
#include "../main/state_manager.h"
#include "../main/transaction_manager.h"

static DeviceState current_state;
static bool helmet_on = true;
static NotificationType last_notification;

DeviceState get_device_state() { return current_state; }
bool get_helmet_state() { return helmet_on; }
void start_new_therapy(uint16_t duration) { (void)duration; }
void set_brightness_of_region(uint8_t region, uint8_t level) { (void)region; (void)level; }
void add_and_send_notification_info(NotificationType type) { last_notification = type; }
void stop_lasers(){}
void reset_passed_therapy_duration(){}
void update_passed_therapy_duration(){}
void start_therapy(bool cont) {(void)cont;}
uint8_t get_temperature(){ return 25; }
void send_info_message(MessageType t, uint8_t* d, size_t l){ (void)t;(void)d;(void)l; }

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
