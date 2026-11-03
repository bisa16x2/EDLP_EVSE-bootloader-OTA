#include "wifi_task.h"

#include <stddef.h>

#include "OTA_WIFI/BSP/bsp_wifi.h"

#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_wifi_task_port.h"

static bool s_initialized = false;

static volatile bool s_wifi_connected = false;
static volatile bool s_network_ready = false;

static bool wifiTaskConfigIsValid(const wifi_task_config_t *config) {
    if (config == NULL) return false;
    if ((config->ssid == NULL) || (config->ssid[0] == '\0')) return false;
    if ((config->password == NULL) || (config->password[0] == '\0')) return false;

    return true;
}

bool wifiTaskInit(const wifi_task_config_t *config) {
    if (s_initialized) return true;

    if (!wifiTaskConfigIsValid(config)) return false;

    s_wifi_connected = false;
    s_network_ready = false;

    if (!bspWifiInit()) return false;
    if (!bspWifiConfigure(config->ssid, config->password)) return false;
    if (!bspWifiStart()) return false;

    s_initialized = true;

    return true;
}

void wifiTaskProcess(void) {
    if (!s_initialized) return;

    /*
     * ESP8266 / Wi-Fi State Machine
     */
    bspWifiProcess();

    /*
     * OTA ↔ WiFi Network Bridge
     * WiFi-side Worker
     */
    netWifiTaskPortWorkerProcess();

    s_wifi_connected = bspWifiIsConnected();
    s_network_ready = bspWifiIsNetworkReady();
}

bool wifiTaskIsConnected(void) { 
    return s_wifi_connected; 
}

bool wifiTaskIsNetworkReady(void) { 
    return s_network_ready; 
}