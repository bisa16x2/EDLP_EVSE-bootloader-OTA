#ifndef EVSE_APP_TASK_OTA__OTA_BOOT_REQUEST_H
#define EVSE_APP_TASK_OTA__OTA_BOOT_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

bool otaBootRequestInit(uint32_t reset_delay_ms);

// Bootloader 진입 예약
bool otaBootRequestSchedule(void);

// Control Task에서 반복 호출
// return true:  정상 또는 처리할 요청 없음
// return false: Boot Request 기록 실패
bool otaBootRequestProcess(void);

bool otaBootRequestIsPending(void);

#endif // EVSE_APP_TASK_OTA__OTA_BOOT_REQUEST_H