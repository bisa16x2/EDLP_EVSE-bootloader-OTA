#ifndef APP_BSP__BSP_WIFI_H
#define APP_BSP__BSP_WIFI_H

#include <stdbool.h>
#include <stdint.h>

// tables
typedef enum {
    BSP_WIFI_STATE_UNINITIALIZED = 0,
    BSP_WIFI_STATE_IDLE,

    BSP_WIFI_STATE_RESETTING,
    BSP_WIFI_STATE_TESTING,
    BSP_WIFI_STATE_DISABLING_ECHO,
    BSP_WIFI_STATE_SETTING_STATION_MODE,
    BSP_WIFI_STATE_JOINING_AP,

    BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION,
    BSP_WIFI_STATE_NETWORK_READY,   // LwM2M/UDP connection

    BSP_WIFI_STATE_RETRY_WAIT,
    BSP_WIFI_STATE_FAULT
} bsp_wifi_state_t;

typedef struct {
    bsp_wifi_state_t state;

    bool initialized;
    bool configured;
    bool started;

    bool wifi_connected;

    uint32_t retry_count;
    uint32_t rx_overflow_count;
} bsp_wifi_status_t;

// functions
bool bspWifiInit(void);
bool bspWifiConfigure(const char *ssid, const char *password);
bool bspWifiStart(void);
void bspWifiProcess(void);

bool bspWifiIsConnected(void);
bool bspWifiIsNetworkReady(void);

bsp_wifi_status_t bspWifiGetStatus(void);

#endif // APP_BSP__BSP_WIFI_H 
