#include "binary_message_encoder.h"

#include <string.h>

size_t encode_device_info_message_binary(const DeviceInfoMessage *m, uint8_t out[DEVICE_INFO_SIZE])
{
    memcpy(&out[0], m->device_id, 6);

    memcpy(&out[6], m->current_time, 5);

    out[11] = (uint8_t)(m->last_saved_therapy_id >> 8);
    out[12] = (uint8_t)(m->last_saved_therapy_id & 0xFF);

    out[13] = (uint8_t)(m->passed_seconds >> 8);
    out[14] = (uint8_t)(m->passed_seconds & 0xFF);

    return DEVICE_INFO_SIZE;
}

size_t encode_timer_state_info_message_binary(const TimerStateInfoMessage *m, uint8_t out[TIMER_STATE_INFO_SIZE])
{
    if (!m || !out) return 0;

    out[0] = m->type;

    out[1] = (uint8_t)(m->therapy_id >> 8);
    out[2] = (uint8_t)(m->therapy_id & 0xFF);

    out[3] = (uint8_t)(m->duration >> 8);
    out[4] = (uint8_t)(m->duration & 0xFF);

    out[5] = (uint8_t)(m->remaining_seconds >> 8);
    out[6] = (uint8_t)(m->remaining_seconds & 0xFF);

    out[7] = (uint8_t)(m->therapy_passed_seconds >> 8);
    out[8] = (uint8_t)(m->therapy_passed_seconds & 0xFF);

    out[9] = (uint8_t)(m->passed_seconds >> 8);
    out[10] = (uint8_t)(m->passed_seconds & 0xFF);

    return TIMER_STATE_INFO_SIZE;
}


size_t encode_measurement_info_message_binary(const MeasurementInfoMessage *m, uint8_t out[MEASUREMENT_INFO_SIZE])
{
    if (!m || !out) return 0;

    out[0] = m->temperature;
    out[1] = m->humidity;

    out[2] = (uint8_t)(m->passed_seconds >> 8);
    out[3] = (uint8_t)(m->passed_seconds & 0xFF);

    return MEASUREMENT_INFO_SIZE;
}


size_t encode_notification_message_binary(const NotificationMessage *m, uint8_t out[NOTIFICATION_INFO_SIZE])
{
    out[0] = m->type;
    out[1] = (uint8_t)(m->passed_seconds >> 8);
    out[2] = (uint8_t)(m->passed_seconds & 0xFF);
    return NOTIFICATION_INFO_SIZE;
}

