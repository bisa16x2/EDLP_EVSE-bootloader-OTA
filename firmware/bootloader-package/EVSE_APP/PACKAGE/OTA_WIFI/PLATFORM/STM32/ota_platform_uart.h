#ifndef OTA_PLATFORM_UART_H
#define OTA_PLATFORM_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef enum {
    OTA_PLATFORM_UART_DEBUG = 0,
    OTA_PLATFORM_UART_WIFI,
    OTA_PLATFORM_UART_MAX
} ota_platform_uart_ch_t;

typedef void (*ota_platform_uart_rx_callback_t)(ota_platform_uart_ch_t ch, uint8_t data);

void otaPlatformUartInit(void);

bool otaPlatformUartSetRxCallback(ota_platform_uart_ch_t ch, ota_platform_uart_rx_callback_t callback);

uint32_t otaPlatformUartWrite(ota_platform_uart_ch_t ch, const uint8_t *data, uint32_t length);
uint32_t otaPlatformUartWriteString(ota_platform_uart_ch_t ch, const char *string);
int32_t otaPlatformUartPrintf(ota_platform_uart_ch_t ch, const char *format, ...);

void otaPlatformUartHandleRxCplt(UART_HandleTypeDef *huart);
void otaPlatformUartHandleError(UART_HandleTypeDef *huart);

#endif // OTA_PLATFORM_UART_H
