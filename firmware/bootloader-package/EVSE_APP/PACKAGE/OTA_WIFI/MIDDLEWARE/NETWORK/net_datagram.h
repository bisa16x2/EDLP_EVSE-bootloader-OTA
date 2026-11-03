#ifndef EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_H
#define EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_H

#include <stdbool.h>
#include <stdint.h>

// tables
typedef enum {
    NET_DATAGRAM_STATE_UNINITIALIZED = 0,
    NET_DATAGRAM_STATE_IDLE,
    NET_DATAGRAM_STATE_OPENING,
    NET_DATAGRAM_STATE_READY,
    NET_DATAGRAM_STATE_CLOSING,
    NET_DATAGRAM_STATE_ERROR
} net_datagram_state_t;

typedef enum {
    NET_DATAGRAM_TX_IDLE = 0,
    NET_DATAGRAM_TX_BUSY,
    NET_DATAGRAM_TX_SUCCESS,
    NET_DATAGRAM_TX_FAILED
} net_datagram_tx_state_t;

typedef enum {
    NET_DATAGRAM_ERROR_NONE = 0,
    NET_DATAGRAM_ERROR_INVALID_ARGUMENT,
    NET_DATAGRAM_ERROR_NETWORK_NOT_READY,
    NET_DATAGRAM_ERROR_NOT_READY,
    NET_DATAGRAM_ERROR_COMMAND_FAILED,
    NET_DATAGRAM_ERROR_TIMEOUT,
    NET_DATAGRAM_ERROR_CONNECTION_LOST,
    NET_DATAGRAM_ERROR_INTERNAL
} net_datagram_error_t;

typedef struct {
    net_datagram_state_t state;
    net_datagram_tx_state_t tx_state;
    net_datagram_error_t last_error;

    bool initialized;
    bool ready;

    uint32_t rx_count;
    uint32_t tx_success_count;
    uint32_t tx_fail_count;
} net_datagram_status_t;

typedef void (*net_datagram_rx_callback_t)(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);

struct net_datagram_port;
typedef struct net_datagram_port net_datagram_port_t;

// functions
bool netDatagramInit(const net_datagram_port_t *port);

bool netDatagramSetRxCallback(net_datagram_rx_callback_t callback);

bool netDatagramOpen(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port);
bool netDatagramClose(void);

bool netDatagramSendTo(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);
void netDatagramProcess(void);

bool netDatagramIsReady(void);
bool netDatagramIsTxBusy(void);

net_datagram_status_t netDatagramGetStatus(void);

#endif // EVSE_APP_MIDDLEWARE_NETWORK_NET_DATAGRAM_H