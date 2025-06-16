#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H
#include <stddef.h>
typedef void* SemaphoreHandle_t;
static inline int xSemaphoreTake(SemaphoreHandle_t x, TickType_t t){(void)x;(void)t;return pdTRUE;}
static inline void xSemaphoreGive(SemaphoreHandle_t x){(void)x;}
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void){return NULL;}
#endif
