#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"

// Minimal test macros
#define TEST_ASSERT_EQUAL_INT(expected, actual) if((expected)!=(actual)) { \
    printf("Assertion failed: %d != %d\n", (expected),(actual)); return 1; }
#define TEST_ASSERT_TRUE(cond) if(!(cond)){ printf("Assertion failed: %s\n", #cond); return 1; }

// === Begin stubs and state ===

#include "../main/transaction_logic.h"
#include "../main/laser_driver_control.h"
#include "../main/timer_management.h"
#include "../main/state_manager.h"
#include "../main/transaction_manager.h"

static DeviceState current_state;
static bool helmet_on = true;
static NotificationType last_notification;
static MessageType last_message_type;
static uint8_t last_sent_payload[64];
static size_t last_sent_len;

DeviceState get_device_state() { return current_state; }
bool get_helmet_state() { return helmet_on; }
uint8_t get_temperature() { return 25; }
uint16_t read_therapy_count() { return 1; }
void start_new_therapy(uint16_t duration) { (void)duration; }
void set_brightness_of_region(uint8_t r, uint8_t b){ (void)r; (void)b; }
void start_therapy(bool cont) { (void)cont; }
void stop_lasers() {}
void update_passed_therapy_duration() {}
void reset_passed_therapy_duration() {}
uint16_t get_passed_duration() { return 10; }

void add_notification_log(uint8_t type, uint16_t passed){ (void)type; (void)passed; }
void add_log(uint8_t type, const uint8_t* d, size_t l, uint16_t p){ (void)type;(void)d;(void)l;(void)p; }

void send_info_message(MessageType type, uint8_t* data, size_t len){
    last_message_type = type;
    last_sent_len = len < sizeof(last_sent_payload) ? len : sizeof(last_sent_payload);
    if(data && len>0) memcpy(last_sent_payload, data, last_sent_len);
}

void add_and_send_notification_info(NotificationType type){
    last_notification = type;
    send_info_message(NOTIFICATION_INFO_MESSAGE, NULL, 0);
}

// === Code under test ===

// === Simple fragment encoder from matching_message_encoder.c ===
#include "../main/matching_message_encoder.c"

static void send_record_fragments(uint16_t therapy_id){
    for(uint16_t i=0;i<=get_fragment_count();i++){
        const uint8_t* frag=get_fragment(i);
        size_t len=get_fragment_length(i);
        if(frag && len>0) send_info_message(RECORDS_INFO_MESSAGE,(uint8_t*)frag,len);
    }
}

// === Tests ===
int main(){
    ActivationMessage a={0};
    for(int i=0;i<6;i++) a.brightness[i]=i;

    // brightness update during active therapy
    current_state=STATE_ACTIVE; a.therapy_duration=0; last_notification=0;
    handle_activation_message(&a);
    TEST_ASSERT_EQUAL_INT(REGIONS_BRIGHTNESS_UPDATED,last_notification);

    // start therapy from inactivity
    current_state=STATE_INACTIVITY; a.therapy_duration=120; last_notification=0;
    handle_activation_message(&a);
    TEST_ASSERT_EQUAL_INT(THERAPY_STARTED_BY_APP,last_notification);

    // status change stop while active
    StatusChangeMessage sc={"STOP",1}; current_state=STATE_ACTIVE; last_notification=0;
    handle_status_change_message(&sc);
    TEST_ASSERT_EQUAL_INT(THERAPY_STOPPED_BY_APP,last_notification);

    // feedback device info ack when active
    FeedbackMessage fb={.type=DEVICE_INFO_MESSAGE_ACK}; current_state=STATE_ACTIVE; last_notification=0;
    handle_feedback_message(&fb);
    TEST_ASSERT_EQUAL_INT(CURRENT_STATE_THERAPY,last_notification);

    // feedback helmet state ack when helmet off
    fb.type=DEVICE_STATE_INFO_MESSAGE_ACK; helmet_on=false; current_state=STATE_INACTIVITY; last_notification=0;
    handle_feedback_message(&fb);
    TEST_ASSERT_EQUAL_INT(HELMET_OFF,last_notification);
    helmet_on=true;

    // fragment encoding and send
    start_encoding_for_new_therapy(5,300,280);
    uint8_t meas[4]={1,2,0x00,10};
    encode_records_of_therapy(5,0x03,4,1,meas);
    send_record_fragments(5);
    TEST_ASSERT_EQUAL_INT(RECORDS_INFO_MESSAGE,last_message_type);
    TEST_ASSERT_TRUE(last_sent_len>0);

    printf("All tests passed.\n");
    return 0;
}

