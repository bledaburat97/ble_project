
#include "therapy_counter.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>

#define TAG "TherapyCounter"

#define COUNTER_PARTITION_NAME "counter_data"
#define MAX_THERAPY_COUNT 16384
#define THERAPY_COUNTER_SUBTYPE 0x83

static const esp_partition_t* counter_partition = NULL;

esp_err_t init_therapy_counter_partition() {
    counter_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, THERAPY_COUNTER_SUBTYPE, COUNTER_PARTITION_NAME);

    if (!counter_partition) {
        ESP_LOGE(TAG, "Counter Partition not found");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void erase_therapy_counter_partition() {
    esp_err_t err = esp_partition_erase_range(counter_partition, 0, counter_partition->size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Counter partition successfully erased.");
    } else {
        ESP_LOGE(TAG, "Failed to erase counter partition: %s", esp_err_to_name(err));
    }
}

uint16_t read_therapy_count() {
    uint16_t therapy_count = 0;

    for (int i = 0; i < 4096; i += 2) {
        uint8_t val1, val2;
        esp_partition_read(counter_partition, i, &val1, 1);
        esp_partition_read(counter_partition, i + 1, &val2, 1);

        //ESP_LOGI(TAG, "val1: %u, val2: %u", val1, val2);

        // Hiç yazılmamış bölgeye geldik, sayaç burada biter
        if (val1 == 0xFF && val2 == 0xFF) {
            break;
        }

        // Uyuşmayan kayıt varsa bozulmuş, burada saymayı durdur
        if (val1 != val2) {
            ESP_LOGW(TAG, "Redundant mismatch at offset %d: val1=0x%02X, val2=0x%02X", i, val1, val2);
            break;
        }

        if (val1 == 0x00) {
            therapy_count += 8;
            continue;
        }

        // Bit say: kaç tane 1 biti 0 yapılmış
        uint8_t bit_cleared = 0;
        for (int bit = 7; bit >= 0; bit--) {
            if (!(val1 & (1 << bit))) {
                bit_cleared++;
            } else {
                break; // ilk 1 görüldü, daha sayma
            }
        }

        therapy_count += bit_cleared;
        break; // bu kayıttan sonra yazım yapılmamış, son geçerli kayıt
    }

    if (therapy_count >= MAX_THERAPY_COUNT) {
        //TODO: 16384 kayıt oluşursa sıfırlanması lazım.
        ESP_LOGE(TAG, "Therapy log limit reached (%d)", therapy_count);
        therapy_count = MAX_THERAPY_COUNT;
    }
    ESP_LOGI(TAG, "therapy_count: %u", therapy_count);

    return therapy_count;

}

esp_err_t write_therapy_count(uint16_t new_count) {
    if (new_count >= 16385 || new_count == 0) {
        return ESP_ERR_NO_MEM; // Tüm alan dolmuş
    }

    int record_index = (new_count - 1) / 8;
    int cleared_bits = (new_count - 1) % 8 + 1;
    uint8_t new_value = 0xFF >> cleared_bits;

    ESP_LOGI(TAG, "new therapy count: %u", new_value);
    uint8_t buffer[2] = {new_value, new_value}; // redundant kayıt
    return esp_partition_write(counter_partition, record_index * 2, buffer, 2);
}