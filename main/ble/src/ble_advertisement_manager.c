#include "../include/ble_internal.h"

// Reklam (ADV) paketini cihaz adı + servis UUID + üretici alanı ile hazırlar.
void build_adv_data(void) {
    const char *name = "BLE-DA-SOLID4";
    uint8_t n = (uint8_t)strlen(name);
    s_adv_len = 0;

    // AD Flags (genelde eklemek iyi pratik: LE General Discoverable + BR/EDR not supported)
    s_adv_raw[s_adv_len++] = 2;        // length
    s_adv_raw[s_adv_len++] = 0x01;     // Flags
    s_adv_raw[s_adv_len++] = 0x06;     // LE General + BR/EDR not supported

    // İsim
    s_adv_raw[s_adv_len++] = (uint8_t)(n + 1);
    s_adv_raw[s_adv_len++] = 0x09;
    memcpy(&s_adv_raw[s_adv_len], name, n);
    s_adv_len += n;

    // 16-bit Service UUIDs (0x03) listesi
    s_adv_raw[s_adv_len++] = 3;        // len = 1(type) + 2(uuid)
    s_adv_raw[s_adv_len++] = 0x03;     // Complete 16-bit UUIDs
    s_adv_raw[s_adv_len++] = (uint8_t)(GATTS_SERVICE_UUID16 & 0xFF);
    s_adv_raw[s_adv_len++] = (uint8_t)(GATTS_SERVICE_UUID16 >> 8);

    // Manufacturer Data Örneği
    s_adv_raw[s_adv_len++] = 3;
    s_adv_raw[s_adv_len++] = 0xFF;
    s_adv_raw[s_adv_len++] = 0xFF;
    s_adv_raw[s_adv_len++] = 0xFF;

    // NOT: Legacy ADV 31 byte sınırı var. İleride ek alan koyarsan s_adv_len'i kontrol et.
}
