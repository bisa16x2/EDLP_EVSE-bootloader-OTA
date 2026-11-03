#include "hw_gpio.h"

static GPIO_PinState hwGpioGetPhysicalState(pinmap_gpio_t gpio, bool active) {
    if (active) return gpio.active_state;

    return gpio.active_state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void hwGpioInit(void) { 
    hwGpioWrite(PINMAP_APP_LED, false);
}

void hwGpioWrite(pinmap_gpio_t gpio, bool active) {
    HAL_GPIO_WritePin(gpio.port, gpio.pin, hwGpioGetPhysicalState(gpio, active));
}

bool hwGpioRead(pinmap_gpio_t gpio) { 
    return HAL_GPIO_ReadPin(gpio.port, gpio.pin) == gpio.active_state; 
}

void hwGpioToggle(pinmap_gpio_t gpio) { 
    HAL_GPIO_TogglePin(gpio.port, gpio.pin); 
}
