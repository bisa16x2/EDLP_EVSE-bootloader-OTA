#ifndef EVSE_APP_COMMON__BOOT_REQUEST_CONFIG_H
#define EVSE_APP_COMMON__BOOT_REQUEST_CONFIG_H

#include <stdint.h>

/*
 * STM32F429 Backup SRAM
 *
 * 0x40024000 ~ 0x40024FFF
 */
#define BOOT_BACKUP_SRAM_START_ADDRESS     0x40024000UL
#define BOOT_BACKUP_SRAM_SIZE              4096U

/*
 * --------------------------------------------------------------------------
 * Bootloader Update Request
 * --------------------------------------------------------------------------
 *
 * +0 : Request Magic
 * +4 : Request Magic Inverse
 */
#define BOOT_UPDATE_REQUEST_MAGIC_OFFSET   0U
#define BOOT_UPDATE_REQUEST_INVERSE_OFFSET 4U

/*
 * --------------------------------------------------------------------------
 * OTA Staging Metadata
 * --------------------------------------------------------------------------
 *
 * +8  : Staging Magic
 * +12 : Staging Magic Inverse
 * +16 : Image Size
 * +20 : Image CRC32
 */
#define BOOT_STAGING_MAGIC_OFFSET          8U
#define BOOT_STAGING_INVERSE_OFFSET        12U
#define BOOT_STAGING_SIZE_OFFSET           16U
#define BOOT_STAGING_CRC32_OFFSET          20U

/*
 * --------------------------------------------------------------------------
 * Bootloader → Application Update Result
 * --------------------------------------------------------------------------
 *
 * +24 : Update Result
 */
#define BOOT_UPDATE_RESULT_OFFSET          24U

/*
 * Application → Bootloader request.
 *
 * 반드시 Bootloader와 동일해야 합니다.
 */
#define BOOT_UPDATE_REQUEST_MAGIC          0x45565345UL
#define BOOT_UPDATE_REQUEST_MAGIC_INVERSE  (~BOOT_UPDATE_REQUEST_MAGIC)

/*
 * Valid staging image metadata.
 *
 * 반드시 Bootloader와 동일해야 합니다.
 */
#define BOOT_STAGING_MAGIC                 0x53544745UL
#define BOOT_STAGING_MAGIC_INVERSE         (~BOOT_STAGING_MAGIC)

/*
 * Bootloader → Application persistent result.
 */
#define BOOT_UPDATE_RESULT_NONE            0x00000000UL
#define BOOT_UPDATE_RESULT_SUCCESS         0x53554343UL
#define BOOT_UPDATE_RESULT_FAILURE         0x4641494CUL

#define BOOT_UPDATE_REQUEST_CLEAR_VALUE    0x00000000UL
#define BOOT_STAGING_CLEAR_VALUE           0x00000000UL

/*
 * --------------------------------------------------------------------------
 * Wakaama CoAP MID persistence
 * --------------------------------------------------------------------------
 *
 * Two-slot journal.
 *
 * Slot A:
 * +28 : Magic
 * +32 : Sequence
 * +36 : Next MID
 * +40 : Next MID Inverse
 * +44 : Commit
 *
 * Slot B:
 * +48 : Magic
 * +52 : Sequence
 * +56 : Next MID
 * +60 : Next MID Inverse
 * +64 : Commit
 *
 * 이전 Slot을 유지한 채 새 Slot을 Commit하므로
 * 기록 도중 Reset되어도 마지막 정상 MID state를 복구할 수 있습니다.
 */

#define WAKAAMA_MID_SLOT_A_OFFSET           28U
#define WAKAAMA_MID_SLOT_B_OFFSET           48U

#define WAKAAMA_MID_SLOT_MAGIC_OFFSET       0U
#define WAKAAMA_MID_SLOT_SEQUENCE_OFFSET    4U
#define WAKAAMA_MID_SLOT_NEXT_OFFSET        8U
#define WAKAAMA_MID_SLOT_NEXT_INV_OFFSET    12U
#define WAKAAMA_MID_SLOT_COMMIT_OFFSET      16U

#define WAKAAMA_MID_MAGIC                   0x4D494431UL
#define WAKAAMA_MID_COMMIT                  (~WAKAAMA_MID_MAGIC)

#define WAKAAMA_MID_CLEAR_VALUE             0x00000000UL

#endif // EVSE_APP_COMMON__BOOT_REQUEST_CONFIG_H