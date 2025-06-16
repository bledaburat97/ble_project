#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"

// Minimal test macros
#define TEST_ASSERT_EQUAL_INT(expected, actual) if((expected)!=(actual)) { \
    printf("Assertion failed: %d != %d\n", (expected),(actual)); return 1; }
#define TEST_ASSERT_TRUE(cond) if(!(cond)){ printf("Assertion failed: %s\n", #cond); return 1; }

// === Begin stubs and structures ===

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
    THERAPY_PAUSED_BY_APP,
    THERAPY_CONTINUED_BY_APP,
    THERAPY_STOPPED_BY_APP,
    REGIONS_BRIGHTNESS_UPDATED,
    CURRENT_STATE_THERAPY,
    CURRENT_STATE_INACTIVITY,
    CURRENT_STATE_TEMP_ALERT,
    CURRENT_STATE_HUM_ALERT,
    HELMET_ON,
    HELMET_OFF,
    ONGOING_THERAPY
} NotificationType;

typedef enum {
    NOTIFICATION_INFO_MESSAGE,
    ACTIVE_THERAPY_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE,
    RECORDS_INFO_MESSAGE
} MessageType;

typedef struct {
    uint16_t therapy_duration;
    uint8_t brightness[6];
} ActivationMessage;

typedef struct {
    char type[10];
    uint16_t therapy_id;
} StatusChangeMessage;

typedef struct {
    uint8_t type;
} FeedbackMessage;

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
static void handle_activation_message(const ActivationMessage* msg){
    for(int i=0;i<6;i++){ /* set brightness stubbed */ }
    if(msg->therapy_duration>0){
        if(get_device_state()==STATE_INACTIVITY || get_device_state()==STATE_ACTIVE){
            if(get_helmet_state()){
                start_new_therapy(msg->therapy_duration);
                add_and_send_notification_info(THERAPY_STARTED_BY_APP);
            }
        }
    }else if(get_device_state()==STATE_ACTIVE){
        add_and_send_notification_info(REGIONS_BRIGHTNESS_UPDATED);
    }
}

static void handle_status_change_message(const StatusChangeMessage* msg){
    if(strcmp(msg->type,"STOP")==0){
        if(get_device_state()==STATE_ACTIVE){
            add_and_send_notification_info(THERAPY_STOPPED_BY_APP);
            stop_lasers();
            reset_passed_therapy_duration();
        }
    } else if(strcmp(msg->type,"PAUSE")==0){
        if(get_device_state()==STATE_ACTIVE){
            add_and_send_notification_info(THERAPY_PAUSED_BY_APP);
            stop_lasers();
            update_passed_therapy_duration();
        }
    } else if(strcmp(msg->type,"CONTINUE")==0){
        if(get_device_state()==STATE_INACTIVITY){
            start_therapy(true);
        }
    }
}

static void handle_feedback_message(const FeedbackMessage* msg){
    if(msg->type==0){ // DEVICE_INFO_MESSAGE_ACK
        NotificationType n;
        if(get_device_state()==STATE_ACTIVE) n=CURRENT_STATE_THERAPY;
        else if(get_device_state()==STATE_INACTIVITY) n=CURRENT_STATE_INACTIVITY;
        else if(get_device_state()==STATE_TEMPERATURE_ALERT) n=CURRENT_STATE_TEMP_ALERT;
        else if(get_device_state()==STATE_HUMIDITY_ALERT) n=CURRENT_STATE_HUM_ALERT; else return;
        add_and_send_notification_info(n);
    } else if(msg->type==1){ // DEVICE_STATE_INFO_MESSAGE_ACK
        add_and_send_notification_info(get_helmet_state()?HELMET_ON:HELMET_OFF);
    } else if(msg->type==2){ // HELMET_STATE_INFO_MESSAGE_ACK
        if(get_device_state()==STATE_ACTIVE) {
            add_and_send_notification_info(ONGOING_THERAPY);
        } else {
            // send measurement info
            uint8_t buf[3]={get_temperature(),0,0};
            send_info_message(MEASUREMENT_INFO_MESSAGE, buf, 3);
        }
    }
}

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
    FeedbackMessage fb={0}; current_state=STATE_ACTIVE; last_notification=0;
    handle_feedback_message(&fb);
    TEST_ASSERT_EQUAL_INT(CURRENT_STATE_THERAPY,last_notification);

    // feedback helmet state ack when helmet off
    fb.type=1; helmet_on=false; current_state=STATE_INACTIVITY; last_notification=0;
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

