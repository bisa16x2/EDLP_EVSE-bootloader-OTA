#include "net_datagram.h"

#include <stddef.h>
#include <string.h>

#include "net_datagram_port.h"

// inners
static const net_datagram_port_t *s_port;
static bool s_initialized;

static bool netDatagramIsPortValid(const net_datagram_port_t *port) {

    if (port == NULL) return false;

    return (port->init != NULL) && (port->set_rx_callback != NULL) && (port->open != NULL) && (port->close != NULL) &&
           (port->send_to != NULL) && (port->process != NULL) && (port->is_ready != NULL) &&
           (port->is_tx_busy != NULL) && (port->get_status != NULL);
}

// functions
bool netDatagramInit(const net_datagram_port_t *port) {

    if (!netDatagramIsPortValid(port)) return false;

    s_port = port;
    s_initialized = false;

    if (!s_port->init()) {
        s_port = NULL;
        return false;
    }

    s_initialized = true;

    return true;
}

bool netDatagramSetRxCallback(net_datagram_rx_callback_t callback) {

    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->set_rx_callback(callback);
}

bool netDatagramOpen(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port) {

    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->open(initial_remote_host, initial_remote_port, local_port);
}

bool netDatagramClose(void) {
    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->close();
}

bool netDatagramSendTo(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {

    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->send_to(data, length, remote_host, remote_port);
}

void netDatagramProcess(void) {
    if ((!s_initialized) || (s_port == NULL)) return;

    s_port->process();
}

bool netDatagramIsReady(void) {
    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->is_ready();
}

bool netDatagramIsTxBusy(void) {
    if ((!s_initialized) || (s_port == NULL)) return false;

    return s_port->is_tx_busy();
}

net_datagram_status_t netDatagramGetStatus(void) {
    net_datagram_status_t status;

    memset(&status, 0, sizeof(status));

    if ((!s_initialized) || (s_port == NULL)) {

        status.state = NET_DATAGRAM_STATE_UNINITIALIZED;

        return status;
    }

    return s_port->get_status();
}