#include "wakaama_network_port.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_datagram.h"
#include "OTA_WIFI/MIDDLEWARE/LWM2M/wakaama_platform_stm32.h"

#define WAKAAMA_NETWORK_HOST_MAX_LENGTH 127U

#define WAKAAMA_NETWORK_RX_MAX_LENGTH   2048U
#define WAKAAMA_NETWORK_RX_QUEUE_DEPTH  4U

// tables
typedef struct {
    bool active;

    uint16_t security_instance_id;

    char remote_host[WAKAAMA_NETWORK_HOST_MAX_LENGTH + 1U];

    uint16_t remote_port;
} wakaama_network_session_t;

typedef struct {
    bool occupied;

    uint16_t length;
    uint16_t remote_port;

    char remote_host[WAKAAMA_NETWORK_HOST_MAX_LENGTH + 1U];

    uint8_t data[WAKAAMA_NETWORK_RX_MAX_LENGTH];
} wakaama_network_rx_item_t;

typedef struct {
    wakaama_network_status_t status;

    uint16_t security_instance_id;

    char server_host[WAKAAMA_NETWORK_HOST_MAX_LENGTH + 1U];

    uint16_t server_port;
    uint16_t local_port;

    lwm2m_context_t *lwm2m_context;

    wakaama_network_datagram_interceptor_t datagram_interceptor;

    void *datagram_interceptor_context;

    wakaama_network_session_t session;

    wakaama_network_rx_item_t rx_queue[WAKAAMA_NETWORK_RX_QUEUE_DEPTH];

    uint8_t rx_head;
    uint8_t rx_tail;
    uint8_t rx_count;
} wakaama_network_context_t;

// inners
static wakaama_network_context_t s_network;

static void wakaamaNetworkResetRxQueue(void) {
    memset(s_network.rx_queue, 0, sizeof(s_network.rx_queue));

    s_network.rx_head = 0U;
    s_network.rx_tail = 0U;
    s_network.rx_count = 0U;
}

static void wakaamaNetworkHandleRx(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {
    wakaama_network_rx_item_t *item;
    size_t remote_host_length;

    if ((data == NULL) || (length == 0U) || (length > WAKAAMA_NETWORK_RX_MAX_LENGTH)) {

        s_network.status.rx_drop_count++;
        return;
    }

    if (!s_network.session.active) {
        s_network.status.rx_drop_count++;
        return;
    }

    if (s_network.rx_count >= WAKAAMA_NETWORK_RX_QUEUE_DEPTH) {

        s_network.status.rx_drop_count++;
        return;
    }

    item = &s_network.rx_queue[s_network.rx_tail];

    if (remote_host == NULL) {
        s_network.status.rx_drop_count++;
        return;
    }

    remote_host_length = strlen(remote_host);

    if ((remote_host_length == 0U) || (remote_host_length > WAKAAMA_NETWORK_HOST_MAX_LENGTH)) {

        s_network.status.rx_drop_count++;
        return;
    }

    memcpy(item->remote_host, remote_host, remote_host_length);

    item->remote_host[remote_host_length] = '\0';

    memcpy(item->data, data, length);

    item->length = length;
    item->remote_port = remote_port;
    item->occupied = true;

    s_network.rx_tail = (uint8_t)((s_network.rx_tail + 1U) % WAKAAMA_NETWORK_RX_QUEUE_DEPTH);

    s_network.rx_count++;

    s_network.status.rx_count++;
}

// functions
bool wakaamaNetworkInit(void) {
    memset(&s_network, 0, sizeof(s_network));

    wakaamaNetworkResetRxQueue();

    if (!netDatagramSetRxCallback(wakaamaNetworkHandleRx)) return false;

    s_network.status.initialized = true;

    return true;
}

bool wakaamaNetworkConfigure(uint16_t security_instance_id, const char *server_host, uint16_t server_port, uint16_t local_port) {
    size_t host_length;

    if (!s_network.status.initialized) return false;

    if ((server_host == NULL) || (server_port == 0U) || (local_port == 0U)) return false;

    host_length = strlen(server_host);

    if ((host_length == 0U) || (host_length > WAKAAMA_NETWORK_HOST_MAX_LENGTH)) return false;

    memset(s_network.server_host, 0, sizeof(s_network.server_host));
    memcpy(s_network.server_host, server_host, host_length);

    s_network.server_host[host_length] = '\0';

    s_network.security_instance_id = security_instance_id;

    s_network.server_port = server_port;
    s_network.local_port = local_port;

    s_network.status.configured = true;

    return true;
}

bool wakaamaNetworkOpen(void) {
    net_datagram_status_t datagram_status;

    if ((!s_network.status.initialized) || (!s_network.status.configured)) return false;

    datagram_status = netDatagramGetStatus();

    if (datagram_status.ready) {
        s_network.status.socket_ready = true;
        return true;
    }

    if (datagram_status.state == NET_DATAGRAM_STATE_OPENING) return true;
    if (datagram_status.state == NET_DATAGRAM_STATE_CLOSING) return false;

    return netDatagramOpen(s_network.server_host, s_network.server_port, s_network.local_port);
}

bool wakaamaNetworkAttachLwm2mContext(lwm2m_context_t *context) {
    if ((!s_network.status.initialized) || (context == NULL)) return false;

    s_network.lwm2m_context = context;

    return true;
}

bool wakaamaNetworkResetLwm2mRuntime(void) {
    if (!s_network.status.initialized) return false;

    s_network.lwm2m_context = NULL;

    memset(&s_network.session, 0, sizeof(s_network.session));

    s_network.status.session_active = false;

    wakaamaNetworkResetRxQueue();

    s_network.status.socket_ready = netDatagramIsReady();

    return true;
}

bool wakaamaNetworkSetDatagramInterceptor(wakaama_network_datagram_interceptor_t interceptor, void *context) {
    if (!s_network.status.initialized) return false;

    s_network.datagram_interceptor = interceptor;
    s_network.datagram_interceptor_context = context;

    return true;
}

void wakaamaNetworkProcess(void) {
    wakaama_network_rx_item_t *item;

    s_network.status.socket_ready = netDatagramIsReady();

    if ((s_network.lwm2m_context == NULL) || (!s_network.session.active)) return;

    while (s_network.rx_count > 0U) {
        bool consumed = false;

        item = &s_network.rx_queue[s_network.rx_head];

        if (item->occupied) {
            if (s_network.datagram_interceptor != NULL) {
                consumed = s_network.datagram_interceptor(item->data,
                                                          item->length,
                                                          item->remote_host,
                                                          item->remote_port,
                                                          s_network.datagram_interceptor_context);
            }

            if (!consumed) {
                if (item->remote_port == s_network.session.remote_port)

                    lwm2m_handle_packet(s_network.lwm2m_context, item->data, item->length, (void *)&s_network.session);
                else s_network.status.rx_drop_count++;
            }
        }

        memset(item, 0, sizeof(*item));

        s_network.rx_head = (uint8_t)((s_network.rx_head + 1U) % WAKAAMA_NETWORK_RX_QUEUE_DEPTH);

        s_network.rx_count--;
    }
}

bool wakaamaNetworkIsReady(void) {
    s_network.status.socket_ready = netDatagramIsReady();

    return s_network.status.socket_ready;
}

wakaama_network_status_t wakaamaNetworkGetStatus(void) {
    s_network.status.socket_ready = netDatagramIsReady();
    s_network.status.session_active = s_network.session.active;

    return s_network.status;
}

void *wakaamaNetworkGetUserData(void) { return (void *)&s_network; }

/* -------------------------------------------------------------------------- */
/* Wakaama-required transport callbacks                                        */
/* -------------------------------------------------------------------------- */

void *lwm2m_connect_server(uint16_t secObjInstID, void *userData) {
    wakaama_network_context_t *context;
    size_t host_length;

    context = (wakaama_network_context_t *)userData;

    if (context == NULL) return NULL;

    if ((!context->status.initialized) || (!context->status.configured)) return NULL;

    if (secObjInstID != context->security_instance_id) return NULL;

    if (!netDatagramIsReady()) return NULL;

    memset(&context->session, 0, sizeof(context->session));

    host_length = strlen(context->server_host);

    memcpy(context->session.remote_host, context->server_host, host_length);

    context->session.remote_host[host_length] = '\0';
    context->session.security_instance_id = secObjInstID;
    context->session.remote_port = context->server_port;
    context->session.active = true;
    
    context->status.session_active = true;

    return (void *)&context->session;
}

void lwm2m_close_connection(void *sessionH, void *userData) {
    wakaama_network_context_t *context;

    context = (wakaama_network_context_t *)userData;

    if ((context == NULL) || (sessionH == NULL)) return;

    if (sessionH != (void *)&context->session) return;

    context->session.active = false;
    context->status.session_active = false;
}

uint8_t lwm2m_buffer_send(void *sessionH, uint8_t *buffer, size_t length, void *userData) {
    wakaama_network_context_t *context;
    wakaama_network_session_t *session;

    context = (wakaama_network_context_t *)userData;
    session = (wakaama_network_session_t *)sessionH;

    if ((context == NULL) || (session == NULL) || (buffer == NULL) || (length == 0U))
        return COAP_500_INTERNAL_SERVER_ERROR;

    if (session != &context->session) {
        context->status.tx_reject_count++;
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if ((!session->active) || (!netDatagramIsReady())) {
        context->status.tx_reject_count++;
        return COAP_503_SERVICE_UNAVAILABLE;
    }

    if (length > UINT16_MAX) {
        context->status.tx_reject_count++;
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (context->lwm2m_context == NULL) {
        context->status.tx_reject_count++;
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (!wakaamaPlatformCheckpointNextMid(context->lwm2m_context)) {
        context->status.tx_reject_count++;
        return COAP_500_INTERNAL_SERVER_ERROR;
    }

    if (!netDatagramSendTo(buffer, (uint16_t)length, session->remote_host, session->remote_port)) {
        context->status.tx_reject_count++;
        return COAP_503_SERVICE_UNAVAILABLE;
    }

    context->status.tx_accept_count++;

    return COAP_NO_ERROR;
}

bool lwm2m_session_is_equal(void *session1, void *session2, void *userData) {
    (void)userData;

    return (session1 == session2);
}

void lwm2m_session_remove(void *sessionH) {
    (void)sessionH;
}