#include "hw_uart.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pinmap.h"

#define HW_UART_PRINTF_BUFFER_SIZE 160U
#define HW_UART_TX_TIMEOUT_MS      100U

typedef struct {
    uint8_t rx_data;
    hw_uart_rx_callback_t rx_callback;
} hw_uart_context_t;

static hw_uart_context_t s_uart_context[HW_UART_MAX];

static bool hwUartIsValidChannel(hw_uart_ch_t ch) { return ch >= HW_UART_DEBUG && ch < HW_UART_MAX; }

static UART_HandleTypeDef *hwUartGetHandle(hw_uart_ch_t ch) {
    switch (ch) {
        case HW_UART_DEBUG: return PINMAP_DEBUG_UART_HANDLE;
        case HW_UART_MAX:
        default:            return NULL;
    }
}

static bool hwUartStartReceive(hw_uart_ch_t ch) {
    UART_HandleTypeDef *huart;

    if (!hwUartIsValidChannel(ch)) return false;

    huart = hwUartGetHandle(ch);

    if ((huart == NULL) || (s_uart_context[ch].rx_callback == NULL)) return false;

    return HAL_UART_Receive_IT(huart, &s_uart_context[ch].rx_data, 1U) == HAL_OK;
}

void hwUartInit(void) {
    uint32_t index;

    for (index = 0U; index < HW_UART_MAX; index++) {
        s_uart_context[index].rx_data = 0U;
        s_uart_context[index].rx_callback = NULL;
    }
}

bool hwUartSetRxCallback(hw_uart_ch_t ch, hw_uart_rx_callback_t callback) {
    UART_HandleTypeDef *huart;

    if (!hwUartIsValidChannel(ch)) return false;

    huart = hwUartGetHandle(ch);

    if (huart == NULL) return false;

    (void)HAL_UART_AbortReceive(huart);

    s_uart_context[ch].rx_callback = callback;
    s_uart_context[ch].rx_data = 0U;

    if (callback == NULL) return true;

    return hwUartStartReceive(ch);
}

uint32_t hwUartWrite(hw_uart_ch_t ch, const uint8_t *data, uint32_t length) {
    UART_HandleTypeDef *huart;

    if ((!hwUartIsValidChannel(ch)) || (data == NULL) || (length == 0U) || (length > UINT16_MAX)) return 0U;

    huart = hwUartGetHandle(ch);

    if (huart == NULL) return 0U;

    if (HAL_UART_Transmit(huart, (uint8_t *)data, (uint16_t)length, HW_UART_TX_TIMEOUT_MS) != HAL_OK) return 0U;

    return length;
}

uint32_t hwUartWriteString(hw_uart_ch_t ch, const char *string) {
    if (string == NULL) return 0U;

    return hwUartWrite(ch, (const uint8_t *)string, (uint32_t)strlen(string));
}

int32_t hwUartPrintf(hw_uart_ch_t ch, const char *format, ...) {
    char buffer[HW_UART_PRINTF_BUFFER_SIZE];
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

    return hwUartWrite(ch, (const uint8_t *)buffer, length) == length ? (int32_t)length : -1;
}