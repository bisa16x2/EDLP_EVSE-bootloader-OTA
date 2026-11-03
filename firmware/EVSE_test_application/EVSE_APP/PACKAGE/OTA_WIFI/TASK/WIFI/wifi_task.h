#ifndef EVSE_APP_TASK_WIFI_WIFI_TASK_H
#define EVSE_APP_TASK_WIFI_WIFI_TASK_H

#include <stdbool.h>

typedef struct {
    const char *ssid;
    const char *password;
} wifi_task_config_t;

bool wifiTaskInit(const wifi_task_config_t *config);

void wifiTaskProcess(void);

bool wifiTaskIsConnected(void);

bool wifiTaskIsNetworkReady(void);

#endif // EVSE_APP_TASK_WIFI_WIFI_TASK_H