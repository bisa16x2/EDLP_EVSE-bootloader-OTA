#ifndef EVSE_APP_MIDDLEWARE_NETWORK_NET_WIFI_TASK_PORT_H
#define EVSE_APP_MIDDLEWARE_NETWORK_NET_WIFI_TASK_PORT_H

#include "net_datagram.h"

const net_datagram_port_t *netWifiTaskPortGetPort(void);

void netWifiTaskPortWorkerProcess(void);

#endif // EVSE_APP_MIDDLEWARE_NETWORK_NET_WIFI_TASK_PORT_H