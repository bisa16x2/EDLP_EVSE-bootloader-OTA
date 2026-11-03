#ifndef EVSE_APP_BSP__BSP_OTA_STORAGE_H
#define EVSE_APP_BSP__BSP_OTA_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_OTA_STORAGE_PROGRAM_UNIT_SIZE 4U

// tables
typedef enum {
    BSP_OTA_STORAGE_STATUS_OK = 0,

    BSP_OTA_STORAGE_STATUS_INVALID_ARGUMENT,
    BSP_OTA_STORAGE_STATUS_NOT_INITIALIZED,

    BSP_OTA_STORAGE_STATUS_OUT_OF_RANGE,
    BSP_OTA_STORAGE_STATUS_ALIGNMENT_ERROR,

    BSP_OTA_STORAGE_STATUS_FLASH_ERROR,
    BSP_OTA_STORAGE_STATUS_VERIFY_ERROR
} bsp_ota_storage_status_t;

typedef struct {
    bsp_ota_storage_status_t status;

    uint32_t address;
    uint32_t sector;
    uint32_t hal_error;
} bsp_ota_storage_error_info_t;

// functions
bool bspOtaStorageInit(void);

uint32_t bspOtaStorageGetStartAddress(void);

uint32_t bspOtaStorageGetCapacity(void);

bool bspOtaStorageIsImageSizeValid(size_t image_size);

bsp_ota_storage_status_t bspOtaStorageErase(size_t image_size, bsp_ota_storage_error_info_t *error_info);
bsp_ota_storage_status_t bspOtaStorageWrite(size_t offset, const uint8_t *data, size_t length, bool final_chunk, bsp_ota_storage_error_info_t *error_info);

bool bspOtaStorageRead(size_t offset, void *data, size_t length);

#endif