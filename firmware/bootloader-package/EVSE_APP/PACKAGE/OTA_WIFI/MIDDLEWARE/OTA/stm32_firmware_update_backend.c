#include "stm32_firmware_update_backend.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OTA_WIFI/BSP/bsp_ota_storage.h"
#include "OTA_WIFI/BSP/bsp_boot.h"
#include "OTA_WIFI/COMMON/ota_memory_config.h"

#define STM32_OTA_CRC32_INITIAL_VALUE        0xFFFFFFFFUL

#define STM32_OTA_CRC32_FINAL_XOR_VALUE      0xFFFFFFFFUL

#define STM32_OTA_CRC32_REFLECTED_POLYNOMIAL 0xEDB88320UL

#define STM32_OTA_RECOVERY_READ_CHUNK_SIZE 256U

static uint32_t stm32FirmwareBackendCrc32Update(uint32_t crc, const uint8_t *data, size_t length) {
    size_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++) {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
            if ((crc & 1U) != 0U) crc = (crc >> 1U) ^ STM32_OTA_CRC32_REFLECTED_POLYNOMIAL;
            else                  crc >>= 1U;
    }

    return crc;
}

static bool stm32FirmwareBackendCalculateStagingCrc32(size_t image_size, uint32_t *crc_out) {
    uint8_t buffer[STM32_OTA_RECOVERY_READ_CHUNK_SIZE];

    size_t offset;
    size_t read_length;

    uint32_t crc;

    if ((crc_out == NULL) || (!bspOtaStorageIsImageSizeValid(image_size))) return false;

    offset = 0U;
    crc = STM32_OTA_CRC32_INITIAL_VALUE;

    while (offset < image_size) {
        read_length = image_size - offset;

        if (read_length > sizeof(buffer)) read_length = sizeof(buffer);

        if (!bspOtaStorageRead(offset, buffer, read_length)) return false;

        crc = stm32FirmwareBackendCrc32Update(crc, buffer, read_length);

        offset += read_length;
    }

    *crc_out = crc ^ STM32_OTA_CRC32_FINAL_XOR_VALUE;

    return true;
}

static void stm32FirmwareBackendResetTransfer(stm32_firmware_update_backend_context_t *context) {
    if (context == NULL) return;

    context->expected_size = 0U;
    context->received_size = 0U;

    context->last_write_offset = 0U;
    context->last_write_length = 0U;

    context->running_crc32 = STM32_OTA_CRC32_INITIAL_VALUE;
    context->image_crc32 = 0U;
    context->prepared = false;
    context->package_ready = false;
    context->install_requested = false;
}

static bool stm32FirmwareBackendVectorIsValid(size_t image_size) {
    uint32_t initial_stack;
    uint32_t reset_vector;
    uint32_t reset_address;

    bool main_sram_valid;
    bool ccmram_valid;

    if ((image_size < OTA_VECTOR_TABLE_MIN_SIZE) || (image_size > (size_t)OTA_APPLICATION_CAPACITY)) return false;

    if (!bspOtaStorageRead(0U, &initial_stack, sizeof(initial_stack))) return false;

    if (!bspOtaStorageRead(sizeof(uint32_t), &reset_vector, sizeof(reset_vector))) return false;

    if ((initial_stack & 0x7U) != 0U) return false;

    main_sram_valid = (initial_stack > OTA_SRAM_START_ADDRESS) && (initial_stack <= OTA_SRAM_END_ADDRESS);

    ccmram_valid = (initial_stack > OTA_CCMRAM_START_ADDRESS) && (initial_stack <= OTA_CCMRAM_END_ADDRESS);

    if ((!main_sram_valid) && (!ccmram_valid)) return false;

    if ((reset_vector & 0x1U) == 0U) return false;

    reset_address = reset_vector & ~0x1UL;

    if (reset_address < OTA_APPLICATION_START_ADDRESS) return false;

    if (reset_address >= (OTA_APPLICATION_START_ADDRESS + (uint32_t)image_size)) return false;

    if (reset_address >= OTA_APPLICATION_END_ADDRESS) return false;

    return true;
}

static firmware_backend_status_t prvPrepare(void *backend_context, size_t package_size) {
    stm32_firmware_update_backend_context_t *context;

    bsp_ota_storage_error_info_t storage_error;

    bsp_ota_storage_status_t storage_status;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if ((context == NULL) || (package_size == 0U)) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->prepare_call_count++;

    if (!bspBootClearUpdateRequest()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    if (!bspBootClearUpdateResult()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (!bspOtaStorageIsImageSizeValid(package_size)) return FIRMWARE_BACKEND_STATUS_NO_STORAGE;

    storage_status = bspOtaStorageErase(package_size, &storage_error);

    if (storage_status != BSP_OTA_STORAGE_STATUS_OK) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->expected_size = package_size;
    context->received_size = 0U;

    context->last_write_offset = 0U;
    context->last_write_length = 0U;

    context->running_crc32 = STM32_OTA_CRC32_INITIAL_VALUE;
    context->image_crc32 = 0U;
    context->prepared = true;
    context->package_ready = false;
    context->install_requested = false;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prvWriteChunk(void *backend_context, size_t offset, const uint8_t *data, size_t length) {
    stm32_firmware_update_backend_context_t *context;

    bsp_ota_storage_error_info_t storage_error;

    bsp_ota_storage_status_t storage_status;

    bool final_chunk;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if ((context == NULL) || (data == NULL) || (length == 0U) || (!context->prepared) || context->package_ready)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->write_chunk_call_count++;

    if (offset != context->received_size) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if ((offset > context->expected_size) || (length > (context->expected_size - offset)))
        return FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE;

    if ((offset & (BSP_OTA_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U) return FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE;

    final_chunk = ((offset + length) == context->expected_size);

    if ((!final_chunk) && ((length & (BSP_OTA_STORAGE_PROGRAM_UNIT_SIZE - 1U)) != 0U))
        return FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE;

    storage_status = bspOtaStorageWrite(offset, data, length, final_chunk, &storage_error);

    if (storage_status == BSP_OTA_STORAGE_STATUS_VERIFY_ERROR) return FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE;

    if (storage_status != BSP_OTA_STORAGE_STATUS_OK) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->running_crc32 = stm32FirmwareBackendCrc32Update(context->running_crc32, data, length);
    context->last_write_offset = offset;
    context->last_write_length = length;
    context->received_size += length;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prvFinishDownload(void *backend_context) {
    stm32_firmware_update_backend_context_t *context;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if ((context == NULL) || (!context->prepared) || (context->expected_size == 0U))
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->finish_download_call_count++;
    context->package_ready = false;

    if (context->received_size != context->expected_size) return FIRMWARE_BACKEND_STATUS_INTEGRITY_FAILURE;

    if (!stm32FirmwareBackendVectorIsValid(context->expected_size)) return FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE;

    context->image_crc32 = context->running_crc32 ^ STM32_OTA_CRC32_FINAL_XOR_VALUE;

    if (!bspBootSetStagingMetadata((uint32_t)context->expected_size, context->image_crc32)) {
        (void)bspBootClearStagingMetadata();

        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    context->package_ready = true;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prvInstall(void *backend_context) {
    stm32_firmware_update_backend_context_t *context;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if ((context == NULL) || (!context->prepared) || (!context->package_ready) || (context->expected_size == 0U))
        return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;

    context->install_call_count++;

    if (!bspBootClearUpdateResult()) return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;

    context->install_requested = true;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prvCancel(void *backend_context) {
    stm32_firmware_update_backend_context_t *context;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if (context == NULL) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->cancel_call_count++;

    if (!bspBootClearUpdateRequest()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    stm32FirmwareBackendResetTransfer(context);

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t prvRecoverAfterBoot(void *backend_context, firmware_backend_recovery_info_t *recovery_info) {
    stm32_firmware_update_backend_context_t *context;

    bsp_boot_update_result_t stored_result;

    bsp_boot_staging_metadata_t staging_metadata;

    uint32_t calculated_crc32;

    context = (stm32_firmware_update_backend_context_t *)backend_context;

    if ((context == NULL) || (recovery_info == NULL)) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->recover_after_boot_call_count++;

    memset(recovery_info, 0, sizeof(*recovery_info));

    recovery_info->update_result = FIRMWARE_BACKEND_RECOVERY_NONE;

    recovery_info->package_ready = false;
    recovery_info->package_size = 0U;

    if (!bspBootReadUpdateResult(&stored_result)) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    switch (stored_result) {
        case BSP_BOOT_UPDATE_RESULT_SUCCESS:
            recovery_info->update_result = FIRMWARE_BACKEND_RECOVERY_SUCCESS;
            break;

        case BSP_BOOT_UPDATE_RESULT_FAILURE:
            recovery_info->update_result = FIRMWARE_BACKEND_RECOVERY_ROLLED_BACK;
            break;

        case BSP_BOOT_UPDATE_RESULT_NONE:
        default:
            recovery_info->update_result = FIRMWARE_BACKEND_RECOVERY_NONE;
            break;
    }

    stm32FirmwareBackendResetTransfer(context);

    if (stored_result != BSP_BOOT_UPDATE_RESULT_NONE) {
        if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

        return FIRMWARE_BACKEND_STATUS_OK;
    }

    if (!bspBootReadStagingMetadata(&staging_metadata)) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (!staging_metadata.valid) return FIRMWARE_BACKEND_STATUS_OK;

    if (!bspOtaStorageIsImageSizeValid((size_t)staging_metadata.image_size)) {
        if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

        return FIRMWARE_BACKEND_STATUS_OK;
    }

    if (!stm32FirmwareBackendVectorIsValid((size_t)staging_metadata.image_size)) {
        if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

        return FIRMWARE_BACKEND_STATUS_OK;
    }

    if (!stm32FirmwareBackendCalculateStagingCrc32((size_t)staging_metadata.image_size, &calculated_crc32))
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (calculated_crc32 != staging_metadata.image_crc32) {
        if (!bspBootClearStagingMetadata()) return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

        return FIRMWARE_BACKEND_STATUS_OK;
    }

    context->expected_size = (size_t)staging_metadata.image_size;
    context->received_size = (size_t)staging_metadata.image_size;

    context->last_write_offset = 0U;
    context->last_write_length = 0U;

    context->running_crc32 = staging_metadata.image_crc32 ^ STM32_OTA_CRC32_FINAL_XOR_VALUE;

    context->image_crc32 = staging_metadata.image_crc32;

    context->prepared = true;
    context->package_ready = true;
    context->install_requested = false;

    recovery_info->package_ready = true;
    recovery_info->package_size = (size_t)staging_metadata.image_size;

    return FIRMWARE_BACKEND_STATUS_OK;
}

bool stm32_firmware_update_backend_init(stm32_firmware_update_backend_context_t *context, firmware_update_backend_t *backend) {
    if ((context == NULL) || (backend == NULL)) return false;

    if (!bspBootInit()) return false;
    if (!bspOtaStorageInit()) return false;

    memset(context, 0, sizeof(*context));
    memset(backend, 0, sizeof(*backend));

    context->running_crc32 = STM32_OTA_CRC32_INITIAL_VALUE;

    backend->context = context;
    backend->prepare = prvPrepare;
    backend->write_chunk = prvWriteChunk;
    backend->finish_download = prvFinishDownload;
    backend->install = prvInstall;
    backend->cancel = prvCancel;
    backend->recover_after_boot = prvRecoverAfterBoot;

    return true;
}

bool stm32_firmware_update_backend_is_install_requested(const stm32_firmware_update_backend_context_t *context) {
    if (context == NULL) return false;

    return context->install_requested;
}