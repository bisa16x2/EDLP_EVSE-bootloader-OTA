#ifndef EVSE_APP_MIDDLEWARE_OTA_SERVICE__OTA_SERVICE_H
#define EVSE_APP_MIDDLEWARE_OTA_SERVICE__OTA_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OTA_SERVICE_STATE_UNINITIALIZED = 0,

    OTA_SERVICE_STATE_WAIT_NETWORK,

    OTA_SERVICE_STATE_REGISTERING,
    OTA_SERVICE_STATE_READY,

    OTA_SERVICE_STATE_DOWNLOADING,
    OTA_SERVICE_STATE_DOWNLOADED,
    OTA_SERVICE_STATE_UPDATING,

    OTA_SERVICE_STATE_ERROR
} ota_service_state_t;

typedef struct {
    const char *endpoint_prefix;

    const char *server_host;

    uint16_t server_port;
    uint16_t local_port;

    uint16_t short_server_id;
    uint32_t lifetime_seconds;

} ota_service_config_t;
typedef struct {
    ota_service_state_t state;

    bool initialized;
    bool network_ready;
    bool registered;

    uint8_t update_result;

    uint32_t downloaded_size;
    uint32_t total_size;

    bool download_active;

    uint8_t download_transport_state;
    uint8_t download_retry_count;
    uint8_t download_error;

    uint32_t download_block_number;

} ota_service_status_t;

// functions
bool otaServiceInit(const ota_service_config_t *config);

void otaServiceProcess(bool network_ready);

ota_service_status_t otaServiceGetStatus(void);

bool otaServiceIsInstallPending(void);

#endif // EVSE_APP_MIDDLEWARE_OTA_SERVICE__OTA_SERVICE_H