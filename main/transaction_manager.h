#include <stdint.h>
#include "log_types.h"
#include "json_parser.h"

#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

void add_and_send_notification_info(NotificationType notification_type);
void init_ble();
void on_write_of_record_request_message(const char *data);
void handle_activation_message(const ActivationMessage *msg);
void handle_status_change_message(const StatusChangeMessage *msg);
void handle_feedback_message(const FeedbackMessage *msg);

#endif 