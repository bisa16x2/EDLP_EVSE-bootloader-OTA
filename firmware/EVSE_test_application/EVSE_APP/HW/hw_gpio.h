#ifndef APP_HW__HW_GPIO_H
#define APP_HW__HW_GPIO_H

#include <stdbool.h>

#include "pinmap.h"

void hwGpioInit(void);
void hwGpioWrite(pinmap_gpio_t gpio, bool active);
bool hwGpioRead(pinmap_gpio_t gpio);
void hwGpioToggle(pinmap_gpio_t gpio);

#endif // APP_HW__HW_GPIO_H
