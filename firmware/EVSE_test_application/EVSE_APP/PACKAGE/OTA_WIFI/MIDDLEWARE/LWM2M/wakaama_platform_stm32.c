#include "wakaama_platform_stm32.h"
#include "liblwm2m.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

#include "OTA_WIFI/COMMON/boot_request_config.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_uart.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_time.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_backup.h"

#define WAKAAMA_PLATFORM_LOG_BUFFER_SIZE 256U

typedef struct {
    bool valid;

    uint32_t base_offset;
    uint32_t sequence;

    uint16_t next_mid;
} wakaama_mid_slot_t;

static bool s_time_initialized = false;
static uint32_t s_last_time_ms = 0U;
static uint64_t s_elapsed_time_ms = 0ULL;

static unsigned char wakaamaPlatformToLower(unsigned char value) {
    if ((value >= (unsigned char)'A') && (value <= (unsigned char)'Z'))
        return (unsigned char)(value + ((unsigned char)'a' - (unsigned char)'A'));

    return value;
}

static bool wakaamaPlatformReadMidSlot(uint32_t base_offset, wakaama_mid_slot_t *slot) {
    uint32_t magic;
    uint32_t sequence;

    uint32_t next_mid;
    uint32_t next_mid_inverse;

    uint32_t commit;

    if (slot == NULL) return false;

    memset(slot, 0, sizeof(*slot));

    slot->base_offset = base_offset;

    if (!otaPlatformBackupReadWord(base_offset + WAKAAMA_MID_SLOT_MAGIC_OFFSET, &magic)) return false;
    if (!otaPlatformBackupReadWord(base_offset + WAKAAMA_MID_SLOT_COMMIT_OFFSET, &commit)) return false;

    if ((magic != WAKAAMA_MID_MAGIC) || (commit != WAKAAMA_MID_COMMIT)) {
        slot->valid = false;

        return true;
    }

    if (!otaPlatformBackupReadWord(base_offset + WAKAAMA_MID_SLOT_SEQUENCE_OFFSET, &sequence)) return false;
    if (!otaPlatformBackupReadWord(base_offset + WAKAAMA_MID_SLOT_NEXT_OFFSET, &next_mid)) return false;
    if (!otaPlatformBackupReadWord(base_offset + WAKAAMA_MID_SLOT_NEXT_INV_OFFSET, &next_mid_inverse)) return false;

    if (next_mid > UINT16_MAX) {
        slot->valid = false;

        return true;
    }

    if ((next_mid ^ next_mid_inverse) != UINT32_MAX) {
        slot->valid = false;

        return true;
    }

    slot->sequence = sequence;
    slot->next_mid = (uint16_t)next_mid;

    slot->valid = true;

    return true;
}

static bool wakaamaPlatformSequenceIsNewer(uint32_t lhs, uint32_t rhs) {
    return ((int32_t)(lhs - rhs) > 0);
}

static bool wakaamaPlatformGetLatestMidSlot(wakaama_mid_slot_t *latest, wakaama_mid_slot_t *slot_a, wakaama_mid_slot_t *slot_b) {
    if ((latest == NULL) || (slot_a == NULL) || (slot_b == NULL)) return false;

    if (!wakaamaPlatformReadMidSlot(WAKAAMA_MID_SLOT_A_OFFSET, slot_a)) return false;
    if (!wakaamaPlatformReadMidSlot(WAKAAMA_MID_SLOT_B_OFFSET, slot_b)) return false;

    memset(latest, 0, sizeof(*latest));

    if (slot_a->valid && slot_b->valid) {
        if (wakaamaPlatformSequenceIsNewer(slot_b->sequence, slot_a->sequence)) *latest = *slot_b;
        else *latest = *slot_a;

        return true;
    }

    if (slot_a->valid) {
        *latest = *slot_a;
        return true;
    }

    if (slot_b->valid) {
        *latest = *slot_b;
        return true;
    }

    latest->valid = false;

    return true;
}

static bool wakaamaPlatformWriteMidSlot(uint32_t base_offset, uint32_t sequence, uint16_t next_mid) {
    uint32_t stored_mid;
    uint32_t stored_mid_inverse;

    wakaama_mid_slot_t verify;

    stored_mid = (uint32_t)next_mid;
    stored_mid_inverse = ~stored_mid;

    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_COMMIT_OFFSET, WAKAAMA_MID_CLEAR_VALUE)) return false;
    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_MAGIC_OFFSET, WAKAAMA_MID_CLEAR_VALUE)) return false;

    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_SEQUENCE_OFFSET, sequence)) return false;
    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_NEXT_OFFSET, stored_mid)) return false;
    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_NEXT_INV_OFFSET, stored_mid_inverse)) return false;

    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_MAGIC_OFFSET, WAKAAMA_MID_MAGIC)) return false;
    if (!otaPlatformBackupWriteWord(base_offset + WAKAAMA_MID_SLOT_COMMIT_OFFSET, WAKAAMA_MID_COMMIT)) return false;

    if (!wakaamaPlatformReadMidSlot(base_offset, &verify)) return false;

    if ((!verify.valid) || (verify.sequence != sequence) || (verify.next_mid != next_mid)) return false;

    return true;
}

// functions
void *lwm2m_malloc(size_t size) { 
    return malloc(size); 
}

void lwm2m_free(void *pointer) { 
    free(pointer); 
}

char *lwm2m_strdup(const char *string) {
    size_t length;
    char *copy;

    if (string == NULL) return NULL;

    length = strlen(string) + 1U;

    copy = (char *)lwm2m_malloc(length);

    if (copy == NULL) return NULL;

    memcpy(copy, string, length);

    return copy;
}

int lwm2m_strncmp(const char *string1, const char *string2, size_t length) { 
    return strncmp(string1, string2, length); 
}

int lwm2m_strcasecmp(const char *string1, const char *string2) {
    unsigned char char1;
    unsigned char char2;

    if (string1 == NULL) return (string2 == NULL) ? 0 : -1;

    if (string2 == NULL) return 1;

    while ((*string1 != '\0') || (*string2 != '\0')) {
        char1 = wakaamaPlatformToLower((unsigned char)*string1);
        char2 = wakaamaPlatformToLower((unsigned char)*string2);

        if (char1 != char2) return (char1 < char2) ? -1 : 1;

        if (*string1 != '\0') string1++;
        if (*string2 != '\0') string2++;
    }

    return 0;
}

time_t lwm2m_gettime(void) {
    uint32_t current_time_ms;
    uint32_t elapsed_ms;

    current_time_ms = otaPlatformMillis();

    if (!s_time_initialized) {
        s_last_time_ms = current_time_ms;
        s_elapsed_time_ms = 0ULL;
        s_time_initialized = true;

        return (time_t)0;
    }

    elapsed_ms = (uint32_t)(current_time_ms - s_last_time_ms);

    s_elapsed_time_ms += (uint64_t)elapsed_ms;
    s_last_time_ms = current_time_ms;

    return (time_t)(s_elapsed_time_ms / 1000ULL);
}

int lwm2m_seed(void) {
    uint32_t uid0;
    uint32_t uid1;
    uint32_t uid2;

    uint32_t seed;

    uid0 = *(volatile const uint32_t *)(UID_BASE + 0U);
    uid1 = *(volatile const uint32_t *)(UID_BASE + 4U);
    uid2 = *(volatile const uint32_t *)(UID_BASE + 8U);

    seed = uid0;
    seed ^= (uid1 << 7U) | (uid1 >> 25U);
    seed ^= (uid2 << 13U) | (uid2 >> 19U);

    seed ^= otaPlatformMillis();
    seed ^= SysTick->VAL;

    return (int)(seed & 0x7FFFFFFFU);
}

void lwm2m_printf(const char *format, ...) {
    char buffer[WAKAAMA_PLATFORM_LOG_BUFFER_SIZE];

    va_list args;
    int length;

    if (format == NULL) return;

    va_start(args, format);

    length = vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);

    if (length <= 0) return;

    buffer[sizeof(buffer) - 1U] = '\0';

    (void)otaPlatformUartWriteString(OTA_PLATFORM_UART_DEBUG, buffer);
}

bool wakaamaPlatformCheckpointNextMid(const lwm2m_context_t *context) {
    wakaama_mid_slot_t latest;
    wakaama_mid_slot_t slot_a;
    wakaama_mid_slot_t slot_b;

    uint32_t target_offset;
    uint32_t next_sequence;

    if (context == NULL) return false;

    if (!wakaamaPlatformGetLatestMidSlot(&latest, &slot_a, &slot_b)) return false;

    if (latest.valid && (latest.next_mid == context->nextMID)) return true;

    if (!latest.valid) {
        target_offset = WAKAAMA_MID_SLOT_A_OFFSET;

        next_sequence = 1U;
    } 
    else {
        if (latest.base_offset == WAKAAMA_MID_SLOT_A_OFFSET) target_offset = WAKAAMA_MID_SLOT_B_OFFSET;
        else target_offset = WAKAAMA_MID_SLOT_A_OFFSET;

        next_sequence = latest.sequence + 1U;
    }

    return wakaamaPlatformWriteMidSlot(target_offset, next_sequence, context->nextMID);
}

bool wakaamaPlatformRestoreNextMid(lwm2m_context_t *context) {
    wakaama_mid_slot_t latest;
    wakaama_mid_slot_t slot_a;
    wakaama_mid_slot_t slot_b;

    char log_buffer[96];

    if (context == NULL) return false;

    if (!wakaamaPlatformGetLatestMidSlot(&latest, &slot_a, &slot_b)) return false;

    if (latest.valid) {
        context->nextMID = latest.next_mid;

        (void)snprintf(log_buffer,
                       sizeof(log_buffer),
                       "[WAKAAMA MID] restored next=%u seq=%lu\r\n",
                       (unsigned int)context->nextMID,
                       (unsigned long)latest.sequence);
    } 
    else {
        (void)snprintf(log_buffer,
                       sizeof(log_buffer),
                       "[WAKAAMA MID] initialized next=%u\r\n",
                       (unsigned int)context->nextMID);
    }

    (void)otaPlatformUartWriteString(OTA_PLATFORM_UART_DEBUG, log_buffer);

    return wakaamaPlatformCheckpointNextMid(context);
}