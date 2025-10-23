#include "lp_core_main.h"

#include "esp_log.h"
#include <stdio.h>
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_i2c.h"

void initialize_lp_core(void)
{
    size_t firmware_size = lp_core_main_bin_end - lp_core_main_bin_start;

    ESP_LOGI("LP_CORE_MAIN", "LP-Core firmware boyutu: %d byte", firmware_size);
    ESP_ERROR_CHECK(ulp_lp_core_load_binary(lp_core_main_bin_start, lp_core_main_bin_end - lp_core_main_bin_start));

   ulp_lp_core_cfg_t cfg = {
    .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER, 
    .lp_timer_sleep_duration_us = 100000  // 0.1 saniye uyku süresi
};
    // LP-Core'u başlat
    esp_err_t ret = ulp_lp_core_run(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE("LP_CORE_MAIN", "LP-Core baslatilamadi!");
    }
    else{
        ESP_LOGI("LP_CORE_MAIN", "LP-Core baslatildi!");
    }
}


