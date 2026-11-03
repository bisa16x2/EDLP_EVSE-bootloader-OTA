#include "ota_service.h"

#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#include "OTA_WIFI/MIDDLEWARE/LWM2M/wakaama_client.h"

#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_update_backend.h"
#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_update_service.h"
#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_download_transport.h"
#include "OTA_WIFI/MIDDLEWARE/OTA/stm32_firmware_update_backend.h"

#include "OTA_WIFI/MIDDLEWARE/OTA/TRANSPORT/coap_firmware_download_transport.h"

static firmware_update_service_t s_firmware_update_service;

static firmware_update_backend_t s_firmware_update_backend;

static stm32_firmware_update_backend_context_t s_firmware_backend_context;

static firmware_download_transport_t s_firmware_download_transport;

static ota_service_config_t s_config;

static bool s_initialized;
static bool s_network_ready;

#define OTA_SERVICE_ENDPOINT_NAME_MAX_LENGTH 48U

static char s_endpoint_name[OTA_SERVICE_ENDPOINT_NAME_MAX_LENGTH];

static bool s_wakaama_started;

static bool otaServiceBuildEndpointName(const ota_service_config_t *config) {
    uint32_t uid0;
    uint32_t uid1;
    uint32_t uid2;

    uint32_t device_id;

    int result;

    uid0 = HAL_GetUIDw0();
    uid1 = HAL_GetUIDw1();
    uid2 = HAL_GetUIDw2();

    device_id = uid0 ^ uid1 ^ uid2; // 1U; //

    result = snprintf(s_endpoint_name,
                      sizeof(s_endpoint_name),
                      "%s-%08lX",
                      config->endpoint_prefix,
                      (unsigned long)device_id);

    if (result <= 0) return false;

    if ((size_t)result >= sizeof(s_endpoint_name)) return false;

    return true;
}

static bool otaServiceConfigIsValid(const ota_service_config_t *config) {
    if (config == NULL) return false;

    if ((config->endpoint_prefix == NULL) || (config->endpoint_prefix[0] == '\0')) return false;
    if ((config->server_host == NULL) || (config->server_host[0] == '\0')) return false;

    if (config->server_port == 0U) return false;
    if (config->local_port == 0U) return false;

    if (config->short_server_id == 0U) return false;

    if (config->lifetime_seconds == 0U) return false;

    return true;
}

static ota_service_state_t otaServiceGetMappedState(void) {
    wakaama_client_status_t wakaama_status;

    if (!s_initialized) return OTA_SERVICE_STATE_UNINITIALIZED;

    switch (s_firmware_update_service.state) {
        case FIRMWARE_UPDATE_STATE_DOWNLOADING:     return OTA_SERVICE_STATE_DOWNLOADING;
        case FIRMWARE_UPDATE_STATE_DOWNLOADED:      return OTA_SERVICE_STATE_DOWNLOADED;
        case FIRMWARE_UPDATE_STATE_UPDATING:        return OTA_SERVICE_STATE_UPDATING;
        case FIRMWARE_UPDATE_STATE_IDLE:            break;
        default:                                    return OTA_SERVICE_STATE_ERROR;
    }

    if (!s_network_ready) return OTA_SERVICE_STATE_WAIT_NETWORK;

    wakaama_status = wakaamaClientGetStatus();

    if (wakaama_status.runtime_state == WAKAAMA_CLIENT_RUNTIME_ERROR) return OTA_SERVICE_STATE_ERROR;

    if (wakaama_status.registered && (wakaama_status.runtime_state == WAKAAMA_CLIENT_RUNTIME_READY))
        return OTA_SERVICE_STATE_READY;

    return OTA_SERVICE_STATE_REGISTERING;
}

// functions
bool otaServiceInit(const ota_service_config_t *config) {
    wakaama_client_config_t wakaama_config;

    if (s_initialized) return true;
    if (!otaServiceConfigIsValid(config)) return false;

    memset(&s_config, 0, sizeof(s_config));

    s_config = *config;

    if (!stm32_firmware_update_backend_init(&s_firmware_backend_context, &s_firmware_update_backend)) return false;

    if (!firmware_update_service_init(&s_firmware_update_service, &s_firmware_update_backend)) return false;

    if (firmware_update_service_recover_after_boot(&s_firmware_update_service) != FIRMWARE_UPDATE_SERVICE_STATUS_OK)
        return false;

    if (!coapFirmwareDownloadTransportInit(&s_firmware_download_transport)) return false;

    /*
     * ----------------------------------------------------------------------
     * LwM2M Endpoint Name
     * ----------------------------------------------------------------------
     */
    if (!otaServiceBuildEndpointName(config)) return false;

    /*
     * ----------------------------------------------------------------------
     * Wakaama / LwM2M Client
     * ----------------------------------------------------------------------
     */
    memset(&wakaama_config, 0, sizeof(wakaama_config));

    wakaama_config.endpoint_name = s_endpoint_name;

    wakaama_config.server_host = s_config.server_host;
    wakaama_config.server_port = s_config.server_port;

    wakaama_config.local_port = s_config.local_port;

    wakaama_config.short_server_id = s_config.short_server_id;

    wakaama_config.lifetime_seconds = s_config.lifetime_seconds;

    wakaama_config.firmware_update_service = &s_firmware_update_service;

    wakaama_config.firmware_download_transport = &s_firmware_download_transport;

    if (!wakaamaClientInit(&wakaama_config)) return false;

    s_wakaama_started = false;

    s_network_ready = false;
    s_initialized = true;

    return true;
}

void otaServiceProcess(bool network_ready) {
    if (!s_initialized) return;

    s_network_ready = network_ready;

    if (!s_wakaama_started) {
        if (!s_network_ready) return;

        if (!wakaamaClientStart()) return;

        s_wakaama_started = true;
    }

    wakaamaClientProcess();

    coapFirmwareDownloadTransportProcess();
}

ota_service_status_t otaServiceGetStatus(void) {
    wakaama_client_status_t wakaama_status;

    ota_service_status_t status;

    coap_firmware_download_status_t download_status;

    memset(&status, 0, sizeof(status));

    status.state = OTA_SERVICE_STATE_UNINITIALIZED;

    if (!s_initialized) return status;

    download_status = coapFirmwareDownloadTransportGetStatus();

    wakaama_status = wakaamaClientGetStatus();

    status.state = otaServiceGetMappedState();
    status.initialized = true;
    status.network_ready = s_network_ready;

    status.registered = wakaama_status.registered;

    status.update_result = (uint8_t)s_firmware_update_service.update_result;
    status.downloaded_size = (uint32_t)s_firmware_update_service.download_offset;
    status.total_size = download_status.total_size;

    status.download_active = download_status.active;
    status.download_transport_state = (uint8_t)download_status.state;
    status.download_retry_count = download_status.retry_count;
    status.download_error = (uint8_t)download_status.last_error;
    status.download_block_number = download_status.block_number;

    return status;
}

bool otaServiceIsInstallPending(void) {
    if (!s_initialized) return false;

    return stm32_firmware_update_backend_is_install_requested(&s_firmware_backend_context);
}