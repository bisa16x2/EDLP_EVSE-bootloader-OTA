#ifndef APP_BSP__BSP_LED_H
#define APP_BSP__BSP_LED_H

#include <stdbool.h>
#include <stdint.h>

bool bspLedInit(void);
bool bspLedSetBlinkInterval(uint32_t interval_ms);
void bspLedProcess(void);
uint32_t bspLedGetBlinkInterval(void);

#endif // APP_BSP__BSP_LED_H
