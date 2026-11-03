#ifndef EVSE_BOOT_BSP__BSP_STAGING_H
#define EVSE_BOOT_BSP__BSP_STAGING_H

#include <stdbool.h>
#include <stdint.h>

#include "BSP/bsp_common.h"

bsp_status_t BspStaging_Init(void);

bool BspStaging_IsImageSizeValid(uint32_t image_size);
bool BspStaging_IsRangeValid(uint32_t offset, uint32_t length);

bsp_status_t BspStaging_Read(uint32_t offset, uint8_t *data, uint32_t length);

uint32_t BspStaging_GetCapacity(void);

#endif // EVSE_BOOT_BSP__BSP_STAGING_H
