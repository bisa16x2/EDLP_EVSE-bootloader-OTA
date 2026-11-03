#ifndef APP_COMMON__PINMAP_H
#define APP_COMMON__PINMAP_H

#include <stdint.h>

#include "main.h"
#include "usart.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState active_state;
} pinmap_gpio_t;

#define PINMAP_GPIO(label_, active_state_) \
    ((pinmap_gpio_t){                      \
        .port = label_##_GPIO_Port,        \
        .pin = label_##_Pin,               \
        .active_state = active_state_,     \
    })
    
#define PINMAP_GPIO_ACTIVE_HIGH(label_) PINMAP_GPIO(label_, GPIO_PIN_SET)
#define PINMAP_GPIO_ACTIVE_LOW(label_)  PINMAP_GPIO(label_, GPIO_PIN_RESET)

/* NUCLEO-F429ZI 내부 Blue LED: PB7 / CubeMX Label: APP_LED */
#define PINMAP_APP_LED                  PINMAP_GPIO_ACTIVE_HIGH(APP_LED)

/* USART3: ST-LINK Virtual COM Port, USART6: ESP-01 */
#define PINMAP_DEBUG_UART_HANDLE        (&huart3)

#endif /* APP_COMMON__PINMAP_H */
