#include "ota_platform_time.h"

#include "stm32f4xx_hal.h"

uint32_t otaPlatformMillis(void) { 
    return HAL_GetTick(); 
}