#ifndef LOG_PARTITION_MANAGER_H
#define LOG_PARTITION_MANAGER_H

#include "esp_partition.h"

esp_err_t init_log_partition();
const esp_partition_t* get_log_partition();

#endif