#include "hw.h"

#include "stm32f4xx_hal.h"

#include "hw_gpio.h"
#include "hw_uart.h"

// functions
bool hwInit(void) {
    hwGpioInit();
    hwUartInit();

    return true;
}

uint32_t hwMillis(void) { 
    return HAL_GetTick(); 
}
