#ifndef EVSE_BOOT_APP__AP_BOOT_REMOTE_UPDATE_H
#define EVSE_BOOT_APP__AP_BOOT_REMOTE_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AP_BOOT_REMOTE_UPDATE_ERROR_NONE = 0,
    AP_BOOT_REMOTE_UPDATE_ERROR_NOT_INITIALIZED,
    AP_BOOT_REMOTE_UPDATE_ERROR_METADATA,
    AP_BOOT_REMOTE_UPDATE_ERROR_INVALID_IMAGE_SIZE,
    AP_BOOT_REMOTE_UPDATE_ERROR_INVALID_VECTOR,
    AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_READ,
    AP_BOOT_REMOTE_UPDATE_ERROR_STAGING_CRC,
    AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_START,
    AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_WRITE,
    AP_BOOT_REMOTE_UPDATE_ERROR_INSTALL_FINISH,
    AP_BOOT_REMOTE_UPDATE_ERROR_RESULT_COMMIT,
    AP_BOOT_REMOTE_UPDATE_ERROR_METADATA_CLEAR
} ap_boot_remote_update_error_t;

typedef struct {
    bool initialized;
    ap_boot_remote_update_error_t last_error;
    uint32_t image_size;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;
    uint32_t installed_size;
} ap_boot_remote_update_context_t;

bool ApBootRemoteUpdate_Init(ap_boot_remote_update_context_t *context);
bool ApBootRemoteUpdate_Install(ap_boot_remote_update_context_t *context);

ap_boot_remote_update_error_t ApBootRemoteUpdate_GetLastError(const ap_boot_remote_update_context_t *context);

#endif // EVSE_BOOT_APP__AP_BOOT_REMOTE_UPDATE_H
