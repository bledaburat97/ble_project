#ifndef NOTIFICATION_H
#define NOTIFICATION_H

typedef enum {
    HELMET_OFF = 0x00,           // Helmet removed
    HELMET_ON = 0x01,            // Helmet worn
    WRONG_HELMET_STATUS = 0x02,  // Proximity sensor misread
    TIMER_ENDED = 0x03           // Therapy timer ended
} NotificationType;

#endif 
