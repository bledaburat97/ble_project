#include "../include/ble_internal.h"

//Android tarafında Service UUID filtrelemek için çok yararlı.
void build_adv_data(void) {
    const char *name = "BLE-DA";
    uint8_t n = (uint8_t)strlen(name);
    s_adv_len = 0;

    // AD Flags (genelde eklemek iyi pratik: LE General Discoverable + BR/EDR not supported)
    s_adv_raw[s_adv_len++] = 2;        // length
    s_adv_raw[s_adv_len++] = 0x01;     // Flags
    s_adv_raw[s_adv_len++] = 0x06;     // LE General + BR/EDR not supported

    // Complete Local Name
    s_adv_raw[s_adv_len++] = (uint8_t)(n + 1);
    s_adv_raw[s_adv_len++] = 0x09;
    memcpy(&s_adv_raw[s_adv_len], name, n);
    s_adv_len += n;

    // Complete List of 16-bit Service UUIDs (0x03)
    s_adv_raw[s_adv_len++] = 3;        // len = 1(type) + 2(uuid)
    s_adv_raw[s_adv_len++] = 0x03;     // Complete 16-bit UUIDs
    s_adv_raw[s_adv_len++] = (uint8_t)(GATTS_SERVICE_UUID16 & 0xFF);
    s_adv_raw[s_adv_len++] = (uint8_t)(GATTS_SERVICE_UUID16 >> 8);

    // Manufacturer Specific Data (0xFF): [CompanyID LSB][CompanyID MSB][flags...]
    // CompanyID: 0xFFFF (test/placeholder) – ürün için kendi ID'ni kullan
    s_adv_raw[s_adv_len++] = 3;        // len = 1(type) + 2(data) -> burada 1 bayt flag kullanıyoruz
    s_adv_raw[s_adv_len++] = 0xFF;     // Manufacturer specific
    s_adv_raw[s_adv_len++] = 0xFF;     // Company ID LSB (placeholder)
    s_adv_raw[s_adv_len++] = 0xFF;     // Company ID MSB (placeholder)
    // (Opsiyonel) 1 bayt flag (kritik veri var)
    // Eğer ek alan ihtiyacın olursa length'i artırıp ekstra bayt koyabilirsin
    // s_adv_raw[s_adv_len++] = s_adv_mfg_flags; // uzunluk 4 olmalıydı; basit tutmak için şimdilik CompanyID ile yetinelim.

    // NOT: Legacy ADV 31 byte sınırı var. İleride ek alan koyarsan s_adv_len'i kontrol et.
}