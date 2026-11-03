#ifndef EVSE_APP_BSP__BSP_UDP_H
#define EVSE_APP_BSP__BSP_UDP_H

#include <stdbool.h>
#include <stdint.h>

// tables
typedef enum {
    BSP_UDP_STATE_UNINITIALIZED = 0,
    BSP_UDP_STATE_IDLE,

    BSP_UDP_STATE_ENABLING_IPD_INFO,
    BSP_UDP_STATE_OPENING,

    BSP_UDP_STATE_READY,

    BSP_UDP_STATE_CLOSING,

    BSP_UDP_STATE_ERROR
} bsp_udp_state_t;

typedef enum {
    BSP_UDP_TX_IDLE = 0,
    BSP_UDP_TX_BUSY,
    BSP_UDP_TX_SUCCESS,
    BSP_UDP_TX_FAILED
} bsp_udp_tx_state_t;

typedef enum {
    BSP_UDP_ERROR_NONE = 0,

    BSP_UDP_ERROR_INVALID_ARGUMENT,
    BSP_UDP_ERROR_NETWORK_NOT_READY,
    BSP_UDP_ERROR_NOT_READY,

    BSP_UDP_ERROR_SOCKET_ALREADY_OPEN,

    BSP_UDP_ERROR_COMMAND_FAILED,
    BSP_UDP_ERROR_TIMEOUT,
    BSP_UDP_ERROR_CONNECTION_LOST
} bsp_udp_error_t;

typedef struct {
    bsp_udp_state_t state;
    bsp_udp_tx_state_t tx_state;
    bsp_udp_error_t last_error;

    bool initialized;
    bool ipd_info_enabled;
    bool socket_open;

    uint32_t rx_count;
    uint32_t tx_success_count;
    uint32_t tx_fail_count;
} bsp_udp_status_t;

typedef void (*bsp_udp_rx_callback_t)(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);

// functions
bool bspUdpInit(void);

bool bspUdpSetRxCallback(bsp_udp_rx_callback_t callback);

bool bspUdpOpen(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port);
bool bspUdpClose(void);

bool bspUdpSendTo(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);

void bspUdpProcess(void);

bool bspUdpIsReady(void);
bool bspUdpIsTxBusy(void);

bsp_udp_tx_state_t bspUdpGetTxState(void);
bsp_udp_status_t bspUdpGetStatus(void);

#endif // EVSE_APP_BSP__BSP_UDP_H