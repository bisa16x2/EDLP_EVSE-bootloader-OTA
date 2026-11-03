#include "ota_task.h"
#include "ota_boot_request.h"

#include <stddef.h>

#include "OTA_WIFI/TASK/WIFI/wifi_task.h"

#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_datagram.h"
#include "OTA_WIFI/MIDDLEWARE/OTA_SERVICE/ota_service.h"
#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_wifi_task_port.h"

static bool s_initialized = false;
static volatile bool s_install_commit_requested = false;

static bool otaTaskConfigIsValid(const ota_task_config_t *config) {
    if (config == NULL) return false;

    if ((config->endpoint_prefix == NULL) || (config->endpoint_prefix[0] == '\0')) return false;
    if ((config->server_host == NULL) || (config->server_host[0] == '\0')) return false;

    if ((config->server_port == 0U) || (config->local_port == 0U) || (config->short_server_id == 0U) || (config->lifetime_seconds == 0U)) {
        return false;
    }

    if (config->boot_reset_delay_ms == 0U) return false;

    return true;
}

bool otaTaskInit(const ota_task_config_t *config) {
    ota_service_config_t service_config;

    if (s_initialized) return true;

    if (!otaTaskConfigIsValid(config)) return false;

    if (!netDatagramInit(netWifiTaskPortGetPort())) return false;

    service_config.endpoint_prefix = config->endpoint_prefix;
    service_config.server_host = config->server_host;
    service_config.server_port = config->server_port;
    service_config.local_port = config->local_port;
    service_config.short_server_id = config->short_server_id;

    service_config.lifetime_seconds = config->lifetime_seconds;

    if (!otaServiceInit(&service_config)) return false;

    if (!otaBootRequestInit(config->boot_reset_delay_ms)) return false;

    s_initialized = true;
    s_install_commit_requested = false;

    return true;
}

void otaTaskProcess(void) {
    if (!s_initialized) return;

    /*
     * WiFi → OTA RX
     */
    netDatagramProcess();

    /*
     * LwM2M / CoAP / Firmware Update Runtime
     */
    otaServiceProcess(wifiTaskIsNetworkReady());

    if (s_install_commit_requested) {
        if (!otaBootRequestIsPending()) {
            if (otaBootRequestSchedule()) s_install_commit_requested = false;
        }
    }

    (void)otaBootRequestProcess();
}

bool otaTaskIsInstallPending(void) {
    if (!s_initialized) return false;

    return otaServiceIsInstallPending();
}

bool otaTaskCommitInstall(void) {
    if (!s_initialized) return false;

    if (!otaServiceIsInstallPending()) return false;

    if (s_install_commit_requested) return true;

    s_install_commit_requested = true;

    return true;
}