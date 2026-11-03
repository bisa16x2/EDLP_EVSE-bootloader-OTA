#include "ota_boot_request.h"

#include <stdbool.h>
#include <stdint.h>

#include "OTA_WIFI/BSP/bsp_boot.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_time.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_uart.h"

// inners
static bool s_initialized;
static volatile bool s_request_pending;

static uint32_t s_reset_delay_ms;

static uint32_t s_request_tick;

// functions
bool otaBootRequestInit(uint32_t reset_delay_ms) {
    if (s_initialized) return true;

    if (reset_delay_ms == 0U) return false;

    s_request_pending = false;
    s_request_tick = 0U;

    s_reset_delay_ms = reset_delay_ms;

    s_initialized = true;

    return true;
}

bool otaBootRequestSchedule(void) {
    if (!s_initialized) return false;

    // 이미 예약되어 있다면 동일 요청으로 간주
    // CoAP 등의 네트워크 환경에서는 같은 요청이 중복 도착할 수도 있으므로 idempotent하게 처리
    if (s_request_pending) return true;

    s_request_tick = otaPlatformMillis();
    s_request_pending = true;

    return true;
}

bool otaBootRequestProcess(void) {
    uint32_t elapsed_ms;

    if (!s_initialized) return false;

    if (!s_request_pending) return true;

    elapsed_ms = (uint32_t)(otaPlatformMillis() - s_request_tick);

    if (elapsed_ms < s_reset_delay_ms) return true;

    if (!bspBootSetUpdateRequest()) {
        s_request_pending = false;
        return false;
    }

    (void)otaPlatformUartWriteString(OTA_PLATFORM_UART_DEBUG, "[OTA] Boot request committed, reset\r\n");

    s_request_pending = false;

    bspBootReset();

    return false;
}

bool otaBootRequestIsPending(void) { 
    return s_request_pending; 
}