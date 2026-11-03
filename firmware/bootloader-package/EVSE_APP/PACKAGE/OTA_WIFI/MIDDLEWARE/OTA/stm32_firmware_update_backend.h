#ifndef EVSE_OTA_MW__STM32_FIRMWARE_UPDATE_BACKEND_H
#define EVSE_OTA_MW__STM32_FIRMWARE_UPDATE_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "firmware_update_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t expected_size;
    size_t received_size;

    size_t last_write_offset;
    size_t last_write_length;

    uint32_t running_crc32;
    uint32_t image_crc32;

    uint32_t prepare_call_count;
    uint32_t write_chunk_call_count;
    uint32_t finish_download_call_count;
    uint32_t install_call_count;
    uint32_t cancel_call_count;
    uint32_t recover_after_boot_call_count;

    bool prepared;
    bool package_ready;

    volatile bool install_requested;
} stm32_firmware_update_backend_context_t;

bool stm32_firmware_update_backend_init(stm32_firmware_update_backend_context_t *context, firmware_update_backend_t *backend);

bool stm32_firmware_update_backend_is_install_requested(const stm32_firmware_update_backend_context_t *context);

#ifdef __cplusplus
}
#endif

#endif