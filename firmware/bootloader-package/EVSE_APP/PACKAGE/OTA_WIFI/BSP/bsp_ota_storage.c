#include "bsp_ota_storage.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OTA_WIFI/COMMON/ota_memory_config.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_flash.h"

static bool s_initialized;

static void bspOtaStorageClearError(bsp_ota_storage_error_info_t *error_info) {
    if (error_info == NULL) return;

    memset(error_info, 0, sizeof(*error_info));

    error_info->status = BSP_OTA_STORAGE_STATUS_OK;
    error_info->sector = 0xFFFFFFFFUL;
}

static bsp_ota_storage_status_t bspOtaStorageConvertHwStatus(ota_platform_flash_status_t ota_platform_status) {
    switch (ota_platform_status) {
        case OTA_PLATFORM_FLASH_STATUS_OK:                return BSP_OTA_STORAGE_STATUS_OK;
        case OTA_PLATFORM_FLASH_STATUS_INVALID_ARGUMENT:  return BSP_OTA_STORAGE_STATUS_INVALID_ARGUMENT;
        case OTA_PLATFORM_FLASH_STATUS_OUT_OF_RANGE:      return BSP_OTA_STORAGE_STATUS_OUT_OF_RANGE;
        case OTA_PLATFORM_FLASH_STATUS_ALIGNMENT_ERROR:   return BSP_OTA_STORAGE_STATUS_ALIGNMENT_ERROR;
        case OTA_PLATFORM_FLASH_STATUS_VERIFY_ERROR:      return BSP_OTA_STORAGE_STATUS_VERIFY_ERROR;
        case OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR:
        default:                                return BSP_OTA_STORAGE_STATUS_FLASH_ERROR;
    }
}

static bool bspOtaStorageIsRangeValid(size_t offset, size_t length) {
    if (length == 0U) return false;

    if (offset > (size_t)OTA_STAGING_CAPACITY) return false;

    if (length > ((size_t)OTA_STAGING_CAPACITY - offset)) return false;

    return true;
}

bool bspOtaStorageInit(void) {
    if (s_initialized) return true;

    s_initialized = true;

    return true;
}

uint32_t bspOtaStorageGetStartAddress(void) { 
    return OTA_STAGING_START_ADDRESS; 
}

uint32_t bspOtaStorageGetCapacity(void) { 
    return OTA_STAGING_CAPACITY; 
}

bool bspOtaStorageIsImageSizeValid(size_t image_size) {
    return (image_size >= OTA_VECTOR_TABLE_MIN_SIZE) && (image_size <= (size_t)OTA_STAGING_CAPACITY) &&
           (image_size <= (size_t)OTA_APPLICATION_CAPACITY);
}

bsp_ota_storage_status_t bspOtaStorageErase(size_t image_size, bsp_ota_storage_error_info_t *error_info) {
    ota_platform_flash_error_info_t ota_platform_error;

    ota_platform_flash_status_t ota_platform_status;

    bspOtaStorageClearError(error_info);

    if (!s_initialized) return BSP_OTA_STORAGE_STATUS_NOT_INITIALIZED;

    if (!bspOtaStorageIsImageSizeValid(image_size)) return BSP_OTA_STORAGE_STATUS_OUT_OF_RANGE;

    ota_platform_status = otaPlatformFlashEraseRange(OTA_STAGING_START_ADDRESS, image_size, &ota_platform_error);

    if (error_info != NULL) {
        error_info->status = bspOtaStorageConvertHwStatus(ota_platform_status);
        error_info->address = ota_platform_error.address;
        error_info->sector = ota_platform_error.sector;
        error_info->hal_error = ota_platform_error.hal_error;
    }

    return bspOtaStorageConvertHwStatus(ota_platform_status);
}

bsp_ota_storage_status_t bspOtaStorageWrite(size_t offset, const uint8_t *data, size_t length, bool final_chunk, bsp_ota_storage_error_info_t *error_info) {
    uint32_t address;

    ota_platform_flash_error_info_t ota_platform_error;

    ota_platform_flash_status_t ota_platform_status;

    bspOtaStorageClearError(error_info);

    if (!s_initialized) return BSP_OTA_STORAGE_STATUS_NOT_INITIALIZED;

    if ((data == NULL) || (length == 0U)) return BSP_OTA_STORAGE_STATUS_INVALID_ARGUMENT;

    if (!bspOtaStorageIsRangeValid(offset, length)) return BSP_OTA_STORAGE_STATUS_OUT_OF_RANGE;

    if ((offset & (BSP_OTA_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U) return BSP_OTA_STORAGE_STATUS_ALIGNMENT_ERROR;

    if ((!final_chunk) && ((length & (BSP_OTA_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U))
        return BSP_OTA_STORAGE_STATUS_ALIGNMENT_ERROR;

    address = OTA_STAGING_START_ADDRESS + (uint32_t)offset;

    ota_platform_status = otaPlatformFlashWrite(address, data, length, &ota_platform_error);

    if (error_info != NULL) {
        error_info->status = bspOtaStorageConvertHwStatus(ota_platform_status);
        error_info->address = ota_platform_error.address;
        error_info->sector = ota_platform_error.sector;
        error_info->hal_error = ota_platform_error.hal_error;
    }

    return bspOtaStorageConvertHwStatus(ota_platform_status);
}

bool bspOtaStorageRead(size_t offset, void *data, size_t length) {
    uint32_t address;

    if (!s_initialized) return false;

    if ((data == NULL) || !bspOtaStorageIsRangeValid(offset, length)) return false;

    address = OTA_STAGING_START_ADDRESS + (uint32_t)offset;

    return otaPlatformFlashRead(address, data, length);
}