#include "ota_platform_uart.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "usart.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_stm32_config.h"

#define OTA_PLATFORM_UART_PRINTF_BUFFER_SIZE 160U
#define OTA_PLATFORM_UART_TX_TIMEOUT_MS      100U

typedef struct {
    uint8_t rx_data;
    ota_platform_uart_rx_callback_t rx_callback;
} ota_platform_uart_context_t;

static ota_platform_uart_context_t s_uart_context[OTA_PLATFORM_UART_MAX];

static bool otaPlatformUartIsValidChannel(ota_platform_uart_ch_t ch) { return ch >= OTA_PLATFORM_UART_DEBUG && ch < OTA_PLATFORM_UART_MAX; }

static UART_HandleTypeDef *otaPlatformUartGetHandle(ota_platform_uart_ch_t ch) {
    switch (ch) {
        case OTA_PLATFORM_UART_DEBUG: return OTA_PLATFORM_DEBUG_UART_HANDLE;
        case OTA_PLATFORM_UART_WIFI:  return OTA_PLATFORM_WIFI_UART_HANDLE;
        case OTA_PLATFORM_UART_MAX:
        default:            return NULL;
    }
}

static ota_platform_uart_ch_t otaPlatformUartGetChannel(const UART_HandleTypeDef *huart) {
    if (huart == OTA_PLATFORM_DEBUG_UART_HANDLE) return OTA_PLATFORM_UART_DEBUG;

    if (huart == OTA_PLATFORM_WIFI_UART_HANDLE) return OTA_PLATFORM_UART_WIFI;

    return OTA_PLATFORM_UART_MAX;
}

static bool otaPlatformUartStartReceive(ota_platform_uart_ch_t ch) {
    UART_HandleTypeDef *huart;

    if (!otaPlatformUartIsValidChannel(ch)) return false;

    huart = otaPlatformUartGetHandle(ch);

    if ((huart == NULL) || (s_uart_context[ch].rx_callback == NULL)) return false;

    return HAL_UART_Receive_IT(huart, &s_uart_context[ch].rx_data, 1U) == HAL_OK;
}

void otaPlatformUartInit(void) {
    uint32_t index;

    for (index = 0U; index < OTA_PLATFORM_UART_MAX; index++) {
        s_uart_context[index].rx_data = 0U;
        s_uart_context[index].rx_callback = NULL;
    }
}

bool otaPlatformUartSetRxCallback(ota_platform_uart_ch_t ch, ota_platform_uart_rx_callback_t callback) {
    UART_HandleTypeDef *huart;

    if (!otaPlatformUartIsValidChannel(ch)) return false;

    huart = otaPlatformUartGetHandle(ch);

    if (huart == NULL) return false;

    (void)HAL_UART_AbortReceive(huart);

    s_uart_context[ch].rx_callback = callback;
    s_uart_context[ch].rx_data = 0U;

    if (callback == NULL) return true;

    return otaPlatformUartStartReceive(ch);
}

uint32_t otaPlatformUartWrite(ota_platform_uart_ch_t ch, const uint8_t *data, uint32_t length) {
    UART_HandleTypeDef *huart;

    if ((!otaPlatformUartIsValidChannel(ch)) || (data == NULL) || (length == 0U) || (length > UINT16_MAX)) return 0U;

    huart = otaPlatformUartGetHandle(ch);

    if (huart == NULL) return 0U;

    if (HAL_UART_Transmit(huart, (uint8_t *)data, (uint16_t)length, OTA_PLATFORM_UART_TX_TIMEOUT_MS) != HAL_OK) return 0U;

    return length;
}

uint32_t otaPlatformUartWriteString(ota_platform_uart_ch_t ch, const char *string) {
    if (string == NULL) return 0U;

    return otaPlatformUartWrite(ch, (const uint8_t *)string, (uint32_t)strlen(string));
}

int32_t otaPlatformUartPrintf(ota_platform_uart_ch_t ch, const char *format, ...) {
    char buffer[OTA_PLATFORM_UART_PRINTF_BUFFER_SIZE];
    va_list arguments;
    int result;
    uint32_t length;

    if (format == NULL) return -1;

    va_start(arguments, format);
    result = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (result <= 0) return -1;

    length = (uint32_t)result;

    if (length >= sizeof(buffer)) length = sizeof(buffer) - 1U;

    return otaPlatformUartWrite(ch, (const uint8_t *)buffer, length) == length ? (int32_t)length : -1;
}

void otaPlatformUartHandleRxCplt(UART_HandleTypeDef *huart) {
    ota_platform_uart_ch_t ch;
    ota_platform_uart_rx_callback_t callback;
    uint8_t data;

    ch = otaPlatformUartGetChannel(huart);

    if (!otaPlatformUartIsValidChannel(ch)) return;

    callback = s_uart_context[ch].rx_callback;
    data = s_uart_context[ch].rx_data;

    if (callback != NULL) {
        callback(ch, data);
        (void)otaPlatformUartStartReceive(ch);
    }
}

void otaPlatformUartHandleError(UART_HandleTypeDef *huart) {
    ota_platform_uart_ch_t ch;

    ch = otaPlatformUartGetChannel(huart);

    if (!otaPlatformUartIsValidChannel(ch)) return;

    if (s_uart_context[ch].rx_callback == NULL) return;

    (void)HAL_UART_AbortReceive(huart);
    (void)otaPlatformUartStartReceive(ch);
}
