#ifndef EVSE_APP_TASK_OTA_OTA_TASK_H
#define EVSE_APP_TASK_OTA_OTA_TASK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *endpoint_prefix;
    const char *server_host;

    uint16_t server_port;
    uint16_t local_port;

    uint16_t short_server_id;
    uint32_t lifetime_seconds;

    uint32_t boot_reset_delay_ms;
} ota_task_config_t;

bool otaTaskInit(const ota_task_config_t *config);

void otaTaskProcess(void);

bool otaTaskIsInstallPending(void);

bool otaTaskCommitInstall(void);

#endif // EVSE_APP_TASK_OTA_OTA_TASK_H