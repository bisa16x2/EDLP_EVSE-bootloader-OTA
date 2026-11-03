#include "bsp_boot.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#include "OTA_WIFI/COMMON/boot_request_config.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_backup.h"

static bool s_initialized;

bool bspBootInit(void) {
    uint32_t value;

    if (!otaPlatformBackupInit()) return false;

    if (s_initialized) return true;

    if (!otaPlatformBackupReadWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, &value)) return false;

    s_initialized = true;

    return true;
}

bool bspBootClearUpdateRequest(void) {
    if (!s_initialized) return false;

    if (!otaPlatformBackupWriteWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, BOOT_UPDATE_REQUEST_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, BOOT_UPDATE_REQUEST_CLEAR_VALUE)) return false;

    return true;
}

bool bspBootSetUpdateRequest(void) {
    uint32_t request_magic;
    uint32_t request_inverse;

    if (!s_initialized) return false;

    if (!otaPlatformBackupWriteWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, BOOT_UPDATE_REQUEST_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, BOOT_UPDATE_REQUEST_MAGIC)) return false;

    if (!otaPlatformBackupWriteWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, BOOT_UPDATE_REQUEST_MAGIC_INVERSE)) return false;

    if (!otaPlatformBackupReadWord(BOOT_UPDATE_REQUEST_MAGIC_OFFSET, &request_magic)) return false;
    if (!otaPlatformBackupReadWord(BOOT_UPDATE_REQUEST_INVERSE_OFFSET, &request_inverse)) return false;

    return (request_magic == BOOT_UPDATE_REQUEST_MAGIC) && (request_inverse == BOOT_UPDATE_REQUEST_MAGIC_INVERSE);
}

bool bspBootClearStagingMetadata(void) {
    if (!s_initialized) return false;

    if (!otaPlatformBackupWriteWord(BOOT_STAGING_INVERSE_OFFSET, BOOT_STAGING_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_MAGIC_OFFSET, BOOT_STAGING_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_SIZE_OFFSET, 0U)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_CRC32_OFFSET, 0U)) return false;

    return true;
}

bool bspBootReadStagingMetadata(bsp_boot_staging_metadata_t *metadata) {
    uint32_t staging_magic;
    uint32_t staging_inverse;

    uint32_t image_size;
    uint32_t image_crc32;

    if ((!s_initialized) || (metadata == NULL)) return false;

    memset(metadata, 0, sizeof(*metadata));

    if (!otaPlatformBackupReadWord(BOOT_STAGING_MAGIC_OFFSET, &staging_magic)) return false;
    if (!otaPlatformBackupReadWord(BOOT_STAGING_INVERSE_OFFSET, &staging_inverse)) return false;

    if ((staging_magic != BOOT_STAGING_MAGIC) || (staging_inverse != BOOT_STAGING_MAGIC_INVERSE)) {
        metadata->valid = false;

        return true;
    }

    if (!otaPlatformBackupReadWord(BOOT_STAGING_SIZE_OFFSET, &image_size)) return false;

    if (!otaPlatformBackupReadWord(BOOT_STAGING_CRC32_OFFSET, &image_crc32)) return false;

    metadata->image_size = image_size;
    metadata->image_crc32 = image_crc32;

    metadata->valid = true;

    return true;
}

bool bspBootSetStagingMetadata(uint32_t image_size, uint32_t image_crc32) {
    uint32_t staging_magic;
    uint32_t staging_inverse;

    uint32_t stored_size;
    uint32_t stored_crc32;

    if (!s_initialized) return false;
    if (image_size == 0U) return false;

    if (!otaPlatformBackupWriteWord(BOOT_STAGING_INVERSE_OFFSET, BOOT_STAGING_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_MAGIC_OFFSET, BOOT_STAGING_CLEAR_VALUE)) return false;

    if (!otaPlatformBackupWriteWord(BOOT_STAGING_SIZE_OFFSET, image_size)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_CRC32_OFFSET, image_crc32)) return false;

    if (!otaPlatformBackupWriteWord(BOOT_STAGING_MAGIC_OFFSET, BOOT_STAGING_MAGIC)) return false;
    if (!otaPlatformBackupWriteWord(BOOT_STAGING_INVERSE_OFFSET, BOOT_STAGING_MAGIC_INVERSE)) return false;

    if (!otaPlatformBackupReadWord(BOOT_STAGING_MAGIC_OFFSET, &staging_magic)) return false;
    if (!otaPlatformBackupReadWord(BOOT_STAGING_INVERSE_OFFSET, &staging_inverse)) return false;
    if (!otaPlatformBackupReadWord(BOOT_STAGING_SIZE_OFFSET, &stored_size)) return false;
    if (!otaPlatformBackupReadWord(BOOT_STAGING_CRC32_OFFSET, &stored_crc32)) return false;

    return (staging_magic == BOOT_STAGING_MAGIC) &&
           (staging_inverse == BOOT_STAGING_MAGIC_INVERSE) &&
           (stored_size == image_size) &&
           (stored_crc32 == image_crc32);
}

bool bspBootReadUpdateResult(bsp_boot_update_result_t *result) {
    uint32_t raw_result;

    if ((!s_initialized) || (result == NULL)) return false;

    if (!otaPlatformBackupReadWord(BOOT_UPDATE_RESULT_OFFSET, &raw_result)) return false;

    switch (raw_result) {
        case BOOT_UPDATE_RESULT_SUCCESS:
            *result = BSP_BOOT_UPDATE_RESULT_SUCCESS;
            break;

        case BOOT_UPDATE_RESULT_FAILURE:
            *result = BSP_BOOT_UPDATE_RESULT_FAILURE;
            break;

        case BOOT_UPDATE_RESULT_NONE:
        default:
            *result = BSP_BOOT_UPDATE_RESULT_NONE;
            break;
    }

    return true;
}

bool bspBootClearUpdateResult(void) {
    if (!s_initialized) return false;

    return otaPlatformBackupWriteWord(BOOT_UPDATE_RESULT_OFFSET, BOOT_UPDATE_RESULT_NONE);
}

void bspBootReset(void) {
    __DSB();

    NVIC_SystemReset();

    while (1) {}
}