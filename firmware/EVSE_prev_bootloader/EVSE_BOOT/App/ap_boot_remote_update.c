#include "ap_boot_remote_update.h"

#include <stddef.h>
#include <string.h>

#include "App/ap_boot_update.h"
#include "BSP/bsp_boot.h"
#include "BSP/bsp_staging.h"
#include "Middleware/boot_crc32.h"
#include "Common/boot_config.h"

#define AP_BOOT_REMOTE_UPDATE_CHUNK_SIZE 256U

static ap_boot_update_context_t s_installer;
static uint8_t s_transfer_buffer[AP_BOOT_REMOTE_UPDATE_CHUNK_SIZE];

static bool ApBootRemoteUpdate_IsStagingVectorValid(uint32_t image_size) {
    uint32_t initial_msp;
    uint32_t reset_handler;
    uint32_t reset_address;
    bool main_sram_valid;
    bool ccmram_valid;

    if (image_size < (2U * sizeof(uint32_t))) return false;

    if (BspStaging_Read(0U, s_transfer_buffer, 2U * sizeof(uint32_t)) != BSP_STATUS_OK) return false;

    memcpy(&initial_msp, &s_transfer_buffer[0], sizeof(initial_msp));
    memcpy(&reset_handler, &s_transfer_buffer[sizeof(uint32_t)], sizeof(reset_handler));

    if ((initial_msp & 0x7U) != 0U) return false;

    main_sram_valid = (initial_msp > BOOT_SRAM_START_ADDRESS) && (initial_msp <= BOOT_SRAM_STACK_TOP);
    ccmram_valid = (initial_msp > BOOT_CCMRAM_START_ADDRESS) && (initial_msp <= BOOT_CCMRAM_STACK_TOP);

    if ((!main_sram_valid) && (!ccmram_valid)) return false;
    if ((reset_handler & 0x1U) == 0U) return false;

    reset_address = reset_handler & ~0x1UL;

    return (reset_address >= BOOT_APP_START_ADDRESS) &&
           (reset_address < (BOOT_APP_START_ADDRESS + image_size)) &&
           (reset_address < BOOT_APP_END_ADDRESS);
}

static bool ApBootRemoteUpdate_SetFailed(ap_boot_remote_update_context_t *context,
                                         ap_boot_remote_update_error_t error,
                                         bool commit_failure) {
    if (context == NULL) return false;

    context->last_error = error;

    if (commit_failure) (void)BspBoot_SetUpdateResult(BOOT_UPDATE_RESULT_FAILURE);

    return false;
}

static bool ApBootRemoteUpdate_CalculateStagingCrc(ap_boot_remote_update_context_t *context) {
    uint32_t offset;
    uint32_t length;
    uint32_t running_crc32;

    if (context == NULL) return false;

    offset = 0U;
    running_crc32 = BootCrc32_Begin();

    while (offset < context->image_size) {
        length = context->image_size - offset;
        if (length > sizeof(s_transfer_buffer)) length = sizeof(s_transfer_buffer);

        if (BspStaging_Read(offset, s_transfer_buffer, length) != BSP_STATUS_OK)
            return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_READ, false);

        if (BootCrc32_Update(&running_crc32, s_transfer_buffer, length) != MW_STATUS_OK)
            return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_CRC, false);

        offset += length;
    }

    context->calculated_crc32 = BootCrc32_End(running_crc32);

    if (context->calculated_crc32 != context->expected_crc32)
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_CRC, false);

    return true;
}

bool ApBootRemoteUpdate_Init(ap_boot_remote_update_context_t *context) {
    if (context == NULL) return false;

    memset(context, 0, sizeof(*context));
    memset(&s_installer, 0, sizeof(s_installer));

    if (BspStaging_Init() != BSP_STATUS_OK) return false;
    if (!ApBootUpdate_Init(&s_installer)) return false;

    context->initialized = true;
    context->last_error = AP_BOOT_REMOTE_UPDATE_ERROR_NONE;

    return true;
}

bool ApBootRemoteUpdate_Install(ap_boot_remote_update_context_t *context) {
    bsp_boot_staging_metadata_t metadata;
    uint32_t offset;
    uint32_t length;

    if ((context == NULL) || (!context->initialized))
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_NOT_INITIALIZED, false);

    context->last_error = AP_BOOT_REMOTE_UPDATE_ERROR_NONE;
    context->image_size = 0U;
    context->expected_crc32 = 0U;
    context->calculated_crc32 = 0U;
    context->installed_size = 0U;

    if (BspBoot_GetStagingMetadata(&metadata) != BSP_STATUS_OK)
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_METADATA, true);

    if (!metadata.valid)
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_METADATA, true);

    context->image_size = metadata.image_size;
    context->expected_crc32 = metadata.image_crc32;

    if (!BspStaging_IsImageSizeValid(context->image_size))
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_INVALID_IMAGE_SIZE, true);

    if (!ApBootRemoteUpdate_IsStagingVectorValid(context->image_size))
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_INVALID_VECTOR, true);

    /* 실행 중인 Application을 지우기 전에 staging 전체를 먼저 검증합니다. */
    if (!ApBootRemoteUpdate_CalculateStagingCrc(context)) {
        (void)BspBoot_SetUpdateResult(BOOT_UPDATE_RESULT_FAILURE);
        return false;
    }

    if (BspBoot_SetUpdateResult(BOOT_UPDATE_RESULT_NONE) != BSP_STATUS_OK)
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_RESULT_COMMIT, false);

    if (!ApBootUpdate_Start(&s_installer, context->image_size, context->expected_crc32))
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_START, true);

    offset = 0U;

    while (offset < context->image_size) {
        length = context->image_size - offset;
        if (length > sizeof(s_transfer_buffer)) length = sizeof(s_transfer_buffer);

        if (BspStaging_Read(offset, s_transfer_buffer, length) != BSP_STATUS_OK)
            return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_READ, true);

        if (!ApBootUpdate_WriteData(&s_installer, offset, s_transfer_buffer, length))
            return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_WRITE, true);

        offset += length;
        context->installed_size = offset;
    }

    if (!ApBootUpdate_End(&s_installer))
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_FINISH, true);

    if (BspBoot_SetUpdateResult(BOOT_UPDATE_RESULT_SUCCESS) != BSP_STATUS_OK)
        return ApBootRemoteUpdate_SetFailed(context, AP_BOOT_REMOTE_UPDATE_ERROR_RESULT_COMMIT, false);

    /* SUCCESS를 먼저 commit합니다. Application recovery가 metadata 정리를 한 번 더 수행합니다. */
    (void)BspBoot_ClearStagingMetadata();

    return true;
}

ap_boot_remote_update_error_t ApBootRemoteUpdate_GetLastError(const ap_boot_remote_update_context_t *context) {
    if (context == NULL) return AP_BOOT_REMOTE_UPDATE_ERROR_NOT_INITIALIZED;

    return context->last_error;
}
