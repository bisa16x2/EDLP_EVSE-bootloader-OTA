#include "ota_platform_flash.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#define OTA_PLATFORM_FLASH_START_ADDRESS     0x08000000UL
#define OTA_PLATFORM_FLASH_END_ADDRESS       0x08200000UL

#define OTA_PLATFORM_FLASH_PROGRAM_UNIT_SIZE 4U

#define OTA_PLATFORM_FLASH_NO_SECTOR_ERROR   0xFFFFFFFFUL

typedef struct {
    uint32_t start_address;
    uint32_t end_address;
    uint32_t sector;
} ota_platform_flash_sector_info_t;

/*
 * OTA Staging에서 사용하는 Bank2 sector map
 */
static const ota_platform_flash_sector_info_t s_bank2_sectors[] = {
    {0x08100000UL, 0x08104000UL, FLASH_SECTOR_12},
    {0x08104000UL, 0x08108000UL, FLASH_SECTOR_13},
    {0x08108000UL, 0x0810C000UL, FLASH_SECTOR_14},
    {0x0810C000UL, 0x08110000UL, FLASH_SECTOR_15},
    {0x08110000UL, 0x08120000UL, FLASH_SECTOR_16},
    {0x08120000UL, 0x08140000UL, FLASH_SECTOR_17},
    {0x08140000UL, 0x08160000UL, FLASH_SECTOR_18},
    {0x08160000UL, 0x08180000UL, FLASH_SECTOR_19},
    {0x08180000UL, 0x081A0000UL, FLASH_SECTOR_20},
    {0x081A0000UL, 0x081C0000UL, FLASH_SECTOR_21},
    {0x081C0000UL, 0x081E0000UL, FLASH_SECTOR_22},
    {0x081E0000UL, 0x08200000UL, FLASH_SECTOR_23}
};

static void otaPlatformFlashClearError(ota_platform_flash_error_info_t *error_info) {
    if (error_info == NULL) return;

    memset(error_info, 0, sizeof(*error_info));

    error_info->sector = OTA_PLATFORM_FLASH_NO_SECTOR_ERROR;
}

static bool otaPlatformFlashIsRangeValid(uint32_t address, size_t length) {
    uint32_t end_address;

    if (length == 0U) return false;

    if ((address < OTA_PLATFORM_FLASH_START_ADDRESS) || (address >= OTA_PLATFORM_FLASH_END_ADDRESS)) return false;

    if (length > (size_t)(OTA_PLATFORM_FLASH_END_ADDRESS - address)) return false;

    end_address = address + (uint32_t)length;

    return (end_address > address) && (end_address <= OTA_PLATFORM_FLASH_END_ADDRESS);
}

static int32_t otaPlatformFlashFindBank2Sector(uint32_t address) {
    size_t index;

    for (index = 0U; index < (sizeof(s_bank2_sectors) / sizeof(s_bank2_sectors[0])); index++)
        if ((address >= s_bank2_sectors[index].start_address) && (address < s_bank2_sectors[index].end_address))
            return (int32_t)index;

    return -1;
}

ota_platform_flash_status_t otaPlatformFlashEraseRange(uint32_t start_address, size_t length, ota_platform_flash_error_info_t *error_info) {
    FLASH_EraseInitTypeDef erase;

    uint32_t sector_error;
    uint32_t last_address;

    int32_t first_index;
    int32_t last_index;

    HAL_StatusTypeDef hal_status;
    HAL_StatusTypeDef lock_status;

    otaPlatformFlashClearError(error_info);

    if (length == 0U) return OTA_PLATFORM_FLASH_STATUS_INVALID_ARGUMENT;

    if (!otaPlatformFlashIsRangeValid(start_address, length)) return OTA_PLATFORM_FLASH_STATUS_OUT_OF_RANGE;

    last_address = start_address + (uint32_t)length - 1U;

    first_index = otaPlatformFlashFindBank2Sector(start_address);
    last_index = otaPlatformFlashFindBank2Sector(last_address);

    if ((first_index < 0) || (last_index < 0) || (last_index < first_index)) return OTA_PLATFORM_FLASH_STATUS_OUT_OF_RANGE;

    memset(&erase, 0, sizeof(erase));

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = s_bank2_sectors[first_index].sector;
    erase.NbSectors = (uint32_t)(last_index - first_index + 1);
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    sector_error = OTA_PLATFORM_FLASH_NO_SECTOR_ERROR;

    if (HAL_FLASH_Unlock() != HAL_OK) {

        if (error_info != NULL) {
            error_info->address = start_address;
            error_info->hal_error = HAL_FLASH_GetError();
        }

        return OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR;
    }

    hal_status = HAL_FLASHEx_Erase(&erase, &sector_error);

    lock_status = HAL_FLASH_Lock();

    if ((hal_status != HAL_OK) || (lock_status != HAL_OK) || (sector_error != OTA_PLATFORM_FLASH_NO_SECTOR_ERROR)) {
        if (error_info != NULL) {
            error_info->address = start_address;
            error_info->sector = sector_error;
            error_info->hal_error = HAL_FLASH_GetError();
        }

        return OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR;
    }

    return OTA_PLATFORM_FLASH_STATUS_OK;
}

ota_platform_flash_status_t otaPlatformFlashWrite(uint32_t address, const uint8_t *data, size_t length, ota_platform_flash_error_info_t *error_info) {
    size_t source_offset;

    uint32_t current_address;

    HAL_StatusTypeDef lock_status;

    otaPlatformFlashClearError(error_info);

    if ((data == NULL) || (length == 0U)) return OTA_PLATFORM_FLASH_STATUS_INVALID_ARGUMENT;

    if ((address & (OTA_PLATFORM_FLASH_PROGRAM_UNIT_SIZE - 1U)) != 0U) return OTA_PLATFORM_FLASH_STATUS_ALIGNMENT_ERROR;

    if (!otaPlatformFlashIsRangeValid(address, length)) return OTA_PLATFORM_FLASH_STATUS_OUT_OF_RANGE;

    if (HAL_FLASH_Unlock() != HAL_OK) {
        if (error_info != NULL) {
            error_info->address = address;
            error_info->hal_error = HAL_FLASH_GetError();
        }

        return OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR;
    }

    source_offset = 0U;
    current_address = address;

    while (source_offset < length) {
        uint32_t word = 0xFFFFFFFFUL;

        size_t word_length = length - source_offset;

        if (word_length > OTA_PLATFORM_FLASH_PROGRAM_UNIT_SIZE) word_length = OTA_PLATFORM_FLASH_PROGRAM_UNIT_SIZE;

        memcpy(&word, data + source_offset, word_length);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_address, word) != HAL_OK) {
            if (error_info != NULL) {
                error_info->address = current_address;
                error_info->hal_error = HAL_FLASH_GetError();
            }

            (void)HAL_FLASH_Lock();

            return OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR;
        }

        if (memcmp((const void *)(uintptr_t)current_address, data + source_offset, word_length) != 0) {
            if (error_info != NULL) error_info->address = current_address;

            (void)HAL_FLASH_Lock();

            return OTA_PLATFORM_FLASH_STATUS_VERIFY_ERROR;
        }

        current_address += OTA_PLATFORM_FLASH_PROGRAM_UNIT_SIZE;

        source_offset += word_length;
    }

    lock_status = HAL_FLASH_Lock();

    if (lock_status != HAL_OK) {
        if (error_info != NULL) {
            error_info->address = current_address;
            error_info->hal_error = HAL_FLASH_GetError();
        }

        return OTA_PLATFORM_FLASH_STATUS_FLASH_ERROR;
    }

    return OTA_PLATFORM_FLASH_STATUS_OK;
}

bool otaPlatformFlashRead(uint32_t address, void *data, size_t length) {
    if ((data == NULL) || (length == 0U)) return false;

    if (!otaPlatformFlashIsRangeValid(address, length)) return false;

    memcpy(data, (const void *)(uintptr_t)address, length);

    return true;
}