#include "bsp_led.h"

#include "hw.h"
#include "hw_gpio.h"

static bool s_initialized;
static uint32_t s_blink_interval_ms;
static uint32_t s_previous_toggle_ms;

bool bspLedInit(void) {
    hwGpioWrite(PINMAP_APP_LED, false);

    s_blink_interval_ms = 0U;
    s_previous_toggle_ms = hwMillis();
    s_initialized = true;

    return true;
}

bool bspLedSetBlinkInterval(uint32_t interval_ms) {
    if ((!s_initialized) || (interval_ms == 0U)) return false;

    s_blink_interval_ms = interval_ms;
    s_previous_toggle_ms = hwMillis();

    return true;
}

void bspLedProcess(void) {
    uint32_t now;

    if ((!s_initialized) || (s_blink_interval_ms == 0U)) return;

    now = hwMillis();

    if ((now - s_previous_toggle_ms) < s_blink_interval_ms) return;

    s_previous_toggle_ms = now;
    hwGpioToggle(PINMAP_APP_LED);
}

uint32_t bspLedGetBlinkInterval(void) { 
    return s_blink_interval_ms; 
}
