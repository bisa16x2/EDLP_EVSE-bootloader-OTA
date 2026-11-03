#ifndef EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_CLIENT_H
#define EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_update_service.h"
#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_download_transport.h"

// tables
typedef enum {
    WAKAAMA_CLIENT_RUNTIME_STOPPED = 0,

    WAKAAMA_CLIENT_RUNTIME_WAIT_NETWORK,
    WAKAAMA_CLIENT_RUNTIME_OPENING_NETWORK,

    WAKAAMA_CLIENT_RUNTIME_RUNNING,
    WAKAAMA_CLIENT_RUNTIME_READY,

    WAKAAMA_CLIENT_RUNTIME_ERROR
} wakaama_client_runtime_state_t;

typedef enum {
    WAKAAMA_CLIENT_LWM2M_INITIAL = 0,

    WAKAAMA_CLIENT_LWM2M_BOOTSTRAP_REQUIRED,
    WAKAAMA_CLIENT_LWM2M_BOOTSTRAPPING,

    WAKAAMA_CLIENT_LWM2M_REGISTER_REQUIRED,
    WAKAAMA_CLIENT_LWM2M_REGISTERING,

    WAKAAMA_CLIENT_LWM2M_READY
} wakaama_client_lwm2m_state_t;

typedef enum {
    WAKAAMA_CLIENT_ERROR_NONE = 0,

    WAKAAMA_CLIENT_ERROR_INVALID_ARGUMENT,
    WAKAAMA_CLIENT_ERROR_ALREADY_INITIALIZED,

    WAKAAMA_CLIENT_ERROR_SERVER_URI,
    WAKAAMA_CLIENT_ERROR_NETWORK_INIT,
    WAKAAMA_CLIENT_ERROR_NETWORK_CONFIG,

    WAKAAMA_CLIENT_ERROR_SECURITY_OBJECT,
    WAKAAMA_CLIENT_ERROR_SERVER_OBJECT,
    WAKAAMA_CLIENT_ERROR_DEVICE_OBJECT,
    WAKAAMA_CLIENT_ERROR_FIRMWARE_OBJECT,

    WAKAAMA_CLIENT_ERROR_CONTEXT_INIT,

    WAKAAMA_CLIENT_ERROR_MID_PERSISTENCE,
    
    WAKAAMA_CLIENT_ERROR_CONTEXT_CONFIG,
    WAKAAMA_CLIENT_ERROR_CONTEXT_ATTACH,

    WAKAAMA_CLIENT_ERROR_STEP
} wakaama_client_error_t;

typedef struct {
    const char *endpoint_name;

    const char *server_host;
    uint16_t server_port;

    uint16_t local_port;

    uint16_t short_server_id;

    uint32_t lifetime_seconds;

    firmware_update_service_t *firmware_update_service;
    firmware_download_transport_t *firmware_download_transport;
} wakaama_client_config_t;

typedef struct {
    bool initialized;
    bool started;

    bool network_configured;
    bool network_open_requested;
    bool network_ready;

    bool objects_created;
    bool context_configured;

    bool registered;

    uint16_t short_server_id;
    uint32_t lifetime_seconds;

    wakaama_client_runtime_state_t runtime_state;
    wakaama_client_lwm2m_state_t lwm2m_state;

    int last_step_result;

    wakaama_client_error_t last_error;
} wakaama_client_status_t;

bool wakaamaClientInit(const wakaama_client_config_t *config);

/*
 * LwM2M runtime state machine.
 *
 * 호출 순서:
 *
 * bspWifiProcess()
 * netDatagramProcess()
 * wakaamaClientProcess()
 */
bool wakaamaClientStart(void);

void wakaamaClientProcess(void);

bool wakaamaClientIsReady(void);

// 생성된 Wakaama context와 LwM2M object를 정리합니다.
void wakaamaClientDeinit(void);

bool wakaamaClientIsInitialized(void);

wakaama_client_status_t wakaamaClientGetStatus(void);

#endif // EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_CLIENT_H