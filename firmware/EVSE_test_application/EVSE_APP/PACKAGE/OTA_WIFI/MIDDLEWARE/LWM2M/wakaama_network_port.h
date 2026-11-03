#ifndef EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_NETWORK_PORT_H
#define EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_NETWORK_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "liblwm2m.h"

// tables
typedef struct {
    bool initialized;
    bool configured;
    bool socket_ready;
    bool session_active;

    uint32_t rx_count;
    uint32_t rx_drop_count;

    uint32_t tx_accept_count;
    uint32_t tx_reject_count;
} wakaama_network_status_t;

typedef bool (*wakaama_network_datagram_interceptor_t)(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port, void *context);

// functions
bool wakaamaNetworkInit(void);

bool wakaamaNetworkConfigure(uint16_t security_instance_id, const char *server_host, uint16_t server_port, uint16_t local_port);

bool wakaamaNetworkOpen(void);

bool wakaamaNetworkAttachLwm2mContext(lwm2m_context_t *context);

bool wakaamaNetworkResetLwm2mRuntime(void);

bool wakaamaNetworkSetDatagramInterceptor(wakaama_network_datagram_interceptor_t interceptor, void *context);

void wakaamaNetworkProcess(void);

bool wakaamaNetworkIsReady(void);

wakaama_network_status_t wakaamaNetworkGetStatus(void);

void *wakaamaNetworkGetUserData(void);

#endif // EVSE_APP_MIDDLEWARE_LWM2M__WAKAAMA_NETWORK_PORT_H