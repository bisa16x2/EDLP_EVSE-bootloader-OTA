#ifndef EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_PLATFORM_STM32_H
#define EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_PLATFORM_STM32_H

#include <stdbool.h>

#include "liblwm2m.h"

bool wakaamaPlatformRestoreNextMid(lwm2m_context_t *context);

bool wakaamaPlatformCheckpointNextMid(const lwm2m_context_t *context);

#endif // EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_PLATFORM_STM32_H