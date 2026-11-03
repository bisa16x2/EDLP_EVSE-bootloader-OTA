#include "bsp.h"

#include "bsp_led.h"

bool bspInit(void) {
    if (!bspLedInit()) return false;

    return true;
}
