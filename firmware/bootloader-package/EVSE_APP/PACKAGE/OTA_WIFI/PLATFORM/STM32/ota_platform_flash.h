#ifndef OTA_PLATFORM_FLASH_H
#define OTA_PLATFORM_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// tables
typedef enum {
    OTA_PLATFORM_FLASH_STATUS_OK = 0,

    OTA_PLATFORM_FLASH_STATUS_INVALID_ARGUMENT,
    OTA_PLATFORM_FLASH_STATUS_OUT_OF_RANGE,
    OTA_PLATFORM_FLASH_STATUS_ALIGNMENT_ERROR,

    OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR,
    OTA_PLATFORM_FLASH_STATUS_VERIFY_ERROR
} ota_platform_flash_status_t;

typedef struct {
    uint32_t address;
    uint32_t sector;
    uint32_t hal_error;
} ota_platform_flash_error_info_t;

// functions
ota_platform_flash_status_t otaPlatformFlashEraseRange(uint32_t start_address, size_t length, ota_platform_flash_error_info_t *error_info);

ota_platform_flash_status_t otaPlatformFlashWrite(uint32_t address, const uint8_t *data, size_t length, ota_platform_flash_error_info_t *error_info);

bool otaPlatformFlashRead(uint32_t address, void *data, size_t length);

#endif // OTA_PLATFORM_FLASH_H