#include "net_wifi_task_port.h"

#include <stddef.h>
#include <string.h>

#include "cmsis_os2.h"

#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_datagram_port.h"

#include "OTA_WIFI/BSP/bsp_udp.h"

#define NET_WIFI_TASK_PORT_HOST_MAX_LENGTH      127U
#define NET_WIFI_TASK_PORT_PACKET_MAX_LENGTH    2048U

#define NET_WIFI_TASK_PORT_COMMAND_QUEUE_DEPTH  2U
#define NET_WIFI_TASK_PORT_RX_QUEUE_DEPTH       2U

// tables
typedef enum {
    NET_WIFI_COMMAND_NONE = 0,
    NET_WIFI_COMMAND_OPEN,
    NET_WIFI_COMMAND_CLOSE,
    NET_WIFI_COMMAND_SEND
} net_wifi_command_type_t;


typedef struct {
    net_wifi_command_type_t type;

    char remote_host[NET_WIFI_TASK_PORT_HOST_MAX_LENGTH + 1U];

    uint16_t remote_port;
    uint16_t local_port;

    uint16_t length;

    uint8_t data[NET_WIFI_TASK_PORT_PACKET_MAX_LENGTH];
} net_wifi_command_t;

typedef struct {
    char remote_host[NET_WIFI_TASK_PORT_HOST_MAX_LENGTH + 1U];

    uint16_t remote_port;
    uint16_t length;

    uint8_t data[NET_WIFI_TASK_PORT_PACKET_MAX_LENGTH];
} net_wifi_rx_packet_t;

// inners
static osMessageQueueId_t s_command_queue;
static osMessageQueueId_t s_rx_queue;

static osMutexId_t s_status_mutex;

static net_datagram_rx_callback_t s_network_rx_callback;

static net_datagram_status_t s_cached_status;

static bool netWifiTaskPortCopyString(char *destination, size_t destination_size, const char *source) {
    size_t length;

    if ((destination == NULL) || (source == NULL) || (destination_size == 0U)) return false;

    length = strlen(source);

    if ((length == 0U) || (length >= destination_size)) return false;

    memcpy(destination, source, length + 1U);

    return true;
}

// RX Adapter
static void netWifiTaskPortHandleRx(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {
    net_wifi_rx_packet_t packet;

    if ((data == NULL) || (length == 0U) || (remote_host == NULL)) return;

    if (length > NET_WIFI_TASK_PORT_PACKET_MAX_LENGTH) return;

    memset(&packet, 0, sizeof(packet));

    if (!netWifiTaskPortCopyString(packet.remote_host, sizeof(packet.remote_host), remote_host)) return;

    packet.remote_port = remote_port;
    packet.length = length;

    memcpy(packet.data, data, length);

    (void)osMessageQueuePut(s_rx_queue, &packet, 0U, 0U);
}

// State Mapping
static net_datagram_state_t netWifiTaskPortMapState(bsp_udp_state_t state) {
    switch (state) {
        case BSP_UDP_STATE_IDLE:
            return NET_DATAGRAM_STATE_IDLE;

        case BSP_UDP_STATE_ENABLING_IPD_INFO:
        case BSP_UDP_STATE_OPENING:
            return NET_DATAGRAM_STATE_OPENING;

        case BSP_UDP_STATE_READY:
            return NET_DATAGRAM_STATE_READY;

        case BSP_UDP_STATE_CLOSING:
            return NET_DATAGRAM_STATE_CLOSING;

        case BSP_UDP_STATE_ERROR:
            return NET_DATAGRAM_STATE_ERROR;

        case BSP_UDP_STATE_UNINITIALIZED:
        default:
            return NET_DATAGRAM_STATE_UNINITIALIZED;
    }
}

static net_datagram_tx_state_t netWifiTaskPortMapTxState(bsp_udp_tx_state_t state) {
    switch (state) {
        case BSP_UDP_TX_BUSY:
            return NET_DATAGRAM_TX_BUSY;

        case BSP_UDP_TX_SUCCESS:
            return NET_DATAGRAM_TX_SUCCESS;

        case BSP_UDP_TX_FAILED:
            return NET_DATAGRAM_TX_FAILED;

        case BSP_UDP_TX_IDLE:
        default:
            return NET_DATAGRAM_TX_IDLE;
    }
}

static net_datagram_error_t netWifiTaskPortMapError(bsp_udp_error_t error) {
    switch (error) {
        case BSP_UDP_ERROR_NONE:
            return NET_DATAGRAM_ERROR_NONE;

        case BSP_UDP_ERROR_INVALID_ARGUMENT:
            return NET_DATAGRAM_ERROR_INVALID_ARGUMENT;

        case BSP_UDP_ERROR_NETWORK_NOT_READY:
            return NET_DATAGRAM_ERROR_NETWORK_NOT_READY;

        case BSP_UDP_ERROR_NOT_READY:
        case BSP_UDP_ERROR_SOCKET_ALREADY_OPEN:
            return NET_DATAGRAM_ERROR_NOT_READY;

        case BSP_UDP_ERROR_COMMAND_FAILED:
            return NET_DATAGRAM_ERROR_COMMAND_FAILED;

        case BSP_UDP_ERROR_TIMEOUT:
            return NET_DATAGRAM_ERROR_TIMEOUT;

        case BSP_UDP_ERROR_CONNECTION_LOST:
            return NET_DATAGRAM_ERROR_CONNECTION_LOST;

        default:
            return NET_DATAGRAM_ERROR_INTERNAL;
    }
}

static void netWifiTaskPortUpdateCachedStatus(void) {
    bsp_udp_status_t bsp_status;
    net_datagram_status_t status;

    bsp_status = bspUdpGetStatus();

    memset(&status, 0, sizeof(status));

    status.state = netWifiTaskPortMapState(bsp_status.state);

    status.tx_state = netWifiTaskPortMapTxState(bsp_status.tx_state);

    status.last_error = netWifiTaskPortMapError(bsp_status.last_error);

    status.initialized = bsp_status.initialized;
    status.ready = bspUdpIsReady();

    status.rx_count = bsp_status.rx_count;
    status.tx_success_count = bsp_status.tx_success_count;
    status.tx_fail_count = bsp_status.tx_fail_count;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        s_cached_status = status;

        (void)osMutexRelease(s_status_mutex);
    }
}

// net_datagram Port
static bool netWifiTaskPortInit(void) {
    memset(&s_cached_status, 0, sizeof(s_cached_status));

    s_cached_status.state = NET_DATAGRAM_STATE_UNINITIALIZED;

    s_network_rx_callback = NULL;

    s_command_queue = osMessageQueueNew(NET_WIFI_TASK_PORT_COMMAND_QUEUE_DEPTH, sizeof(net_wifi_command_t), NULL);

    if (s_command_queue == NULL) return false;

    s_rx_queue = osMessageQueueNew(NET_WIFI_TASK_PORT_RX_QUEUE_DEPTH, sizeof(net_wifi_rx_packet_t), NULL);

    if (s_rx_queue == NULL) return false;

    s_status_mutex = osMutexNew(NULL);

    if (s_status_mutex == NULL) return false;

    if (!bspUdpInit()) return false;

    if (!bspUdpSetRxCallback(netWifiTaskPortHandleRx)) return false;

    netWifiTaskPortUpdateCachedStatus();

    return true;
}

static bool netWifiTaskPortSetRxCallback(net_datagram_rx_callback_t callback) {
    s_network_rx_callback = callback;

    return true;
}

static bool netWifiTaskPortOpen(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port) {
    net_wifi_command_t command;

    if ((initial_remote_host == NULL) || (initial_remote_host[0] == '\0') || (initial_remote_port == 0U) ||
        (local_port == 0U)) {

        return false;
    }

    memset(&command, 0, sizeof(command));

    command.type = NET_WIFI_COMMAND_OPEN;

    if (!netWifiTaskPortCopyString(command.remote_host, sizeof(command.remote_host), initial_remote_host)) return false;

    command.remote_port = initial_remote_port;
    command.local_port = local_port;

    if (osMessageQueuePut(s_command_queue, &command, 0U, 0U) != osOK) return false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        s_cached_status.state = NET_DATAGRAM_STATE_OPENING;

        (void)osMutexRelease(s_status_mutex);
    }

    return true;
}

static bool netWifiTaskPortClose(void) {
    net_wifi_command_t command;

    memset(&command, 0, sizeof(command));

    command.type = NET_WIFI_COMMAND_CLOSE;

    if (osMessageQueuePut(s_command_queue, &command, 0U, 0U) != osOK) return false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        s_cached_status.state = NET_DATAGRAM_STATE_CLOSING;

        (void)osMutexRelease(s_status_mutex);
    }

    return true;
}

static bool netWifiTaskPortSendTo(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {
    net_wifi_command_t command;

    net_datagram_status_t status;

    if ((data == NULL) || (length == 0U) || (remote_host == NULL) || (remote_host[0] == '\0') || (remote_port == 0U))

        return false;

    if (length > NET_WIFI_TASK_PORT_PACKET_MAX_LENGTH) return false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) != osOK) return false;

    status = s_cached_status;

    (void)osMutexRelease(s_status_mutex);

    if ((!status.ready) || (status.tx_state == NET_DATAGRAM_TX_BUSY)) return false;

    memset(&command, 0, sizeof(command));

    command.type = NET_WIFI_COMMAND_SEND;

    if (!netWifiTaskPortCopyString(command.remote_host, sizeof(command.remote_host), remote_host)) return false;

    command.remote_port = remote_port;
    command.length = length;

    memcpy(command.data, data, length);

    if (osMessageQueuePut(s_command_queue, &command, 0U, 0U) != osOK) return false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        s_cached_status.tx_state = NET_DATAGRAM_TX_BUSY;

        (void)osMutexRelease(s_status_mutex);
    }

    return true;
}

static void netWifiTaskPortProcess(void) {
    net_wifi_rx_packet_t packet;

    while (osMessageQueueGet(s_rx_queue, &packet, NULL, 0U) == osOK)
        if (s_network_rx_callback != NULL)
            s_network_rx_callback(packet.data, packet.length, packet.remote_host, packet.remote_port);
}

static bool netWifiTaskPortIsReady(void) {
    bool ready = false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        ready = s_cached_status.ready;

        (void)osMutexRelease(s_status_mutex);
    }

    return ready;
}

static bool netWifiTaskPortIsTxBusy(void) {
    bool busy = false;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        busy = (s_cached_status.tx_state == NET_DATAGRAM_TX_BUSY);

        (void)osMutexRelease(s_status_mutex);
    }

    return busy;
}

static net_datagram_status_t netWifiTaskPortGetStatus(void) {
    net_datagram_status_t status;

    memset(&status, 0, sizeof(status));

    status.state = NET_DATAGRAM_STATE_UNINITIALIZED;

    if (osMutexAcquire(s_status_mutex, osWaitForever) == osOK) {
        status = s_cached_status;

        (void)osMutexRelease(s_status_mutex);
    }

    return status;
}

// Port Table
static const net_datagram_port_t s_wifi_task_port = {
    .init = netWifiTaskPortInit,

    .set_rx_callback = netWifiTaskPortSetRxCallback,

    .open = netWifiTaskPortOpen,
    .close = netWifiTaskPortClose,

    .send_to = netWifiTaskPortSendTo,

    .process = netWifiTaskPortProcess,

    .is_ready = netWifiTaskPortIsReady,
    .is_tx_busy = netWifiTaskPortIsTxBusy,

    .get_status = netWifiTaskPortGetStatus
};

static void netWifiTaskPortProcessCommand(void) {
    net_wifi_command_t command;

    if (osMessageQueueGet(s_command_queue, &command, NULL, 0U) != osOK) return;

    switch (command.type) {
        case NET_WIFI_COMMAND_OPEN:
            (void)bspUdpOpen(command.remote_host, command.remote_port, command.local_port);
            break;

        case NET_WIFI_COMMAND_CLOSE:
            (void)bspUdpClose();
            break;

        case NET_WIFI_COMMAND_SEND:
            if (!bspUdpSendTo(command.data, command.length, command.remote_host, command.remote_port)) {;}
            break;

        case NET_WIFI_COMMAND_NONE:
        default:
            break;
    }
}

// functions
const net_datagram_port_t *netWifiTaskPortGetPort(void) { 
    return &s_wifi_task_port; 
}

void netWifiTaskPortWorkerProcess(void) {
    netWifiTaskPortProcessCommand();

    bspUdpProcess();

    netWifiTaskPortUpdateCachedStatus();
}