#ifndef OTA_PLATFORM_BACKUP_H
#define OTA_PLATFORM_BACKUP_H

#include <stdbool.h>
#include <stdint.h>

// functions
bool otaPlatformBackupInit(void);
bool otaPlatformBackupReadWord(uint32_t offset, uint32_t *value);
bool otaPlatformBackupWriteWord(uint32_t offset, uint32_t value);

#endif // OTA_PLATFORM_BACKUP_H