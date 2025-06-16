#ifndef TRANSACTION_LOGIC_H
#define TRANSACTION_LOGIC_H

#include <stddef.h>
#include <stdint.h>
#include "json_parser.h"
#include "log_types.h"

/* MessageType is normally defined in ble_control.h but that header depends on FreeRTOS.
 * Re-declare it here for host unit tests. */
typedef enum {
    RECORDS_INFO_MESSAGE,
    ACTIVE_THERAPY_INFO_MESSAGE,
    MEASUREMENT_INFO_MESSAGE,
    NOTIFICATION_INFO_MESSAGE,
    DEVICE_INFO_MESSAGE
} MessageType;

void handle_activation_message(const ActivationMessage *msg);
void handle_status_change_message(const StatusChangeMessage *msg);
void handle_feedback_message(const FeedbackMessage *msg);

/* Provided by the platform or tests to send serialized info messages */
void send_info_message(MessageType type, uint8_t *data, size_t len);

#endif
