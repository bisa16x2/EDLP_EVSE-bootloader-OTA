#include "bsp_staging.h"

#include <stddef.h>
#include <string.h>

#include "Common/boot_config.h"

static bool s_initialized;

bsp_status_t BspStaging_Init(void) {
    if (BOOT_STAGING_START_ADDRESS != BOOT_APP_END_ADDRESS) return BSP_STATUS_ERROR;
    if (BOOT_STAGING_END_ADDRESS > BOOT_FLASH_STACK_TOP) return BSP_STATUS_ERROR;
    if (BOOT_STAGING_CAPACITY != BOOT_APP_MAX_SIZE) return BSP_STATUS_ERROR;

    s_initialized = true;

    return BSP_STATUS_OK;
}

bool BspStaging_IsImageSizeValid(uint32_t image_size) {
    if (!s_initialized) return false;
    if (image_size < (2U * sizeof(uint32_t))) return false;

    return (image_size <= BOOT_STAGING_CAPACITY) && (image_size <= BOOT_APP_MAX_SIZE);
}

bool BspStaging_IsRangeValid(uint32_t offset, uint32_t length) {
    if ((!s_initialized) || (length == 0U)) return false;
    if (offset >= BOOT_STAGING_CAPACITY) return false;

    return length <= (BOOT_STAGING_CAPACITY - offset);
}

bsp_status_t BspStaging_Read(uint32_t offset, uint8_t *data, uint32_t length) {
    if (!s_initialized) return BSP_STATUS_NOT_INITIALIZED;
    if ((data == NULL) || (!BspStaging_IsRangeValid(offset, length))) return BSP_STATUS_INVALID_ARGUMENT;

    memcpy(data, (const void *)(uintptr_t)(BOOT_STAGING_START_ADDRESS + offset), length);

    return BSP_STATUS_OK;
}

uint32_t BspStaging_GetCapacity(void) {
    return BOOT_STAGING_CAPACITY;
}
