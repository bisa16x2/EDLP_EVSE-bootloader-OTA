#ifndef APP_HW__HW_UART_H
#define APP_HW__HW_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HW_UART_DEBUG = 0,
    HW_UART_MAX
} hw_uart_ch_t;

typedef void (*hw_uart_rx_callback_t)(hw_uart_ch_t ch, uint8_t data);

void hwUartInit(void);

bool hwUartSetRxCallback(hw_uart_ch_t ch, hw_uart_rx_callback_t callback);

uint32_t hwUartWrite(hw_uart_ch_t ch, const uint8_t *data, uint32_t length);
uint32_t hwUartWriteString(hw_uart_ch_t ch, const char *string);
int32_t hwUartPrintf(hw_uart_ch_t ch, const char *format, ...);

#endif // APP_HW__HW_UART_H
