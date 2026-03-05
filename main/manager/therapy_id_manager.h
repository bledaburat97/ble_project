#ifndef THERAPY_ID_MANAGER_H
#define THERAPY_ID_MANAGER_H

#include <stdint.h>

// Aktif/paused terapi için geçerli ID'yi döner.
uint16_t get_current_therapy_id(void);
// Yeni terapi başlatıldığında kullanılacak ID'yi döner.
uint16_t get_new_therapy_id_for_new_therapy(void);
// Son tamamlanan terapinin ID'sini döner.
uint16_t get_last_completed_therapy_id(void);

#endif
