#ifndef EVSE_APP_BSP__BSP_BOOT_H
#define EVSE_APP_BSP__BSP_BOOT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BSP_BOOT_UPDATE_RESULT_NONE = 0,
    BSP_BOOT_UPDATE_RESULT_SUCCESS,
    BSP_BOOT_UPDATE_RESULT_FAILURE
} bsp_boot_update_result_t;

typedef struct {
    bool valid;

    uint32_t image_size;
    uint32_t image_crc32;
} bsp_boot_staging_metadata_t;

bool bspBootInit(void);

bool bspBootSetUpdateRequest(void);

bool bspBootClearUpdateRequest(void);

bool bspBootSetStagingMetadata(uint32_t image_size, uint32_t image_crc32);

bool bspBootReadStagingMetadata(bsp_boot_staging_metadata_t *metadata);

bool bspBootClearStagingMetadata(void);

bool bspBootReadUpdateResult(bsp_boot_update_result_t *result);

bool bspBootClearUpdateResult(void);

void bspBootReset(void);

#endif // EVSE_APP_BSP__BSP_BOOT_H