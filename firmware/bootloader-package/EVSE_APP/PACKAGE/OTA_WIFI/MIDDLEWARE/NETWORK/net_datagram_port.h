#ifndef EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_PORT_H
#define EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_PORT_H

#include "net_datagram.h"

struct net_datagram_port {
    bool (*init)(void);

    bool (*set_rx_callback)(net_datagram_rx_callback_t callback);

    bool (*open)(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port);
    bool (*close)(void);

    bool (*send_to)(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);
    void (*process)(void);

    bool (*is_ready)(void);
    bool (*is_tx_busy)(void);

    net_datagram_status_t (*get_status)(void);
};

#endif // EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_PORT_H