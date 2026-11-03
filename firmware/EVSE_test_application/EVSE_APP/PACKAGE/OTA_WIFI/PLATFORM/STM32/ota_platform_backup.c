#include "ota_platform_backup.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#include "OTA_WIFI/COMMON/boot_request_config.h"

// inners
static bool s_initialized;

static bool otaPlatformBackupIsOffsetValid(uint32_t offset) {
    // 32-bit Word 단위로만 접근
    if ((offset & (sizeof(uint32_t) - 1U)) != 0U) return false;

    // offset 위치에서 4바이트 전체 접근
    if (offset > (BOOT_BACKUP_SRAM_SIZE - sizeof(uint32_t))) return false;

    return true;
}

static volatile uint32_t *otaPlatformBackupGetWordAddress(uint32_t offset) {
    uintptr_t absolute_address;

    absolute_address = (uintptr_t)BOOT_BACKUP_SRAM_START_ADDRESS + (uintptr_t)offset;

    return (volatile uint32_t *)absolute_address;
}

// functions
bool otaPlatformBackupInit(void) {
    if (s_initialized) return true;

    // Backup SRAM 접근 준비
    // 1. PWR Peripheral Clock
    // 2. Backup Domain write access
    // 3. Backup SRAM Clock
    __HAL_RCC_PWR_CLK_ENABLE();

    HAL_PWR_EnableBkUpAccess();

    if ((PWR->CR & PWR_CR_DBP) == 0U) return false;

    __HAL_RCC_BKPSRAM_CLK_ENABLE();

    __DSB();
    __ISB();

    s_initialized = true;

    return true;
}

bool otaPlatformBackupReadWord(uint32_t offset, uint32_t *value) {
    volatile uint32_t *word_address;

    if (value == NULL) return false;

    *value = 0U;

    if (!s_initialized) return false;

    if (!otaPlatformBackupIsOffsetValid(offset)) return false;

    word_address = otaPlatformBackupGetWordAddress(offset);

    __DMB();

    *value = *word_address;

    __DMB();

    return true;
}

bool otaPlatformBackupWriteWord(uint32_t offset, uint32_t value) {
    volatile uint32_t *word_address;

    if (!s_initialized) return false;

    if (!otaPlatformBackupIsOffsetValid(offset)) return false;

    word_address = otaPlatformBackupGetWordAddress(offset);

    *word_address = value;

    // Software Reset 직전에도 실제 Write 완료 보장

    __DSB();

    // Read-back 검증
    if (*word_address != value) return false;

    return true;
}