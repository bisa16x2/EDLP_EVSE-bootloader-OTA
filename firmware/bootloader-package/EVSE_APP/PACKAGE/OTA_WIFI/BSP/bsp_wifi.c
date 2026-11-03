#include "bsp_wifi.h"

#include <stddef.h>
#include <string.h>

#include "OTA_WIFI/DRIVER/esp8266.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_time.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_uart.h"

#define BSP_WIFI_SSID_MAX_LENGTH              32U
#define BSP_WIFI_PASSWORD_MAX_LENGTH          64U

#define BSP_WIFI_RESET_TIMEOUT_MS             5000U
#define BSP_WIFI_COMMAND_TIMEOUT_MS           2000U
#define BSP_WIFI_JOIN_TIMEOUT_MS              40000U

#define BSP_WIFI_RETRY_DELAY_MS               3000U

// tables
typedef struct {
    bsp_wifi_status_t status;

    char ssid[BSP_WIFI_SSID_MAX_LENGTH + 1U];
    char password[BSP_WIFI_PASSWORD_MAX_LENGTH + 1U];

    bool command_pending;
    uint32_t state_start_ms;
} bsp_wifi_context_t;

// inners
static bsp_wifi_context_t s_wifi_context;

static bool bspWifiCopyString(char *destination, size_t destination_size, const char *source) {
    size_t length;

    if ((destination == NULL) || (source == NULL) || (destination_size == 0U)) return false;

    length = strlen(source);

    if ((length == 0U) || (length >= destination_size)) return false;

    memcpy(destination, source, length + 1U);

    return true;
}

static bool bspWifiIsConnectionState(bsp_wifi_state_t state) {
    return (state == BSP_WIFI_STATE_RESETTING) ||
           (state == BSP_WIFI_STATE_TESTING) ||
           (state == BSP_WIFI_STATE_DISABLING_ECHO) ||
           (state == BSP_WIFI_STATE_SETTING_STATION_MODE) ||
           (state == BSP_WIFI_STATE_JOINING_AP) ||
           (state == BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION);
}

static uint32_t bspWifiGetStateTimeout(bsp_wifi_state_t state) {
    if (state == BSP_WIFI_STATE_RESETTING) return BSP_WIFI_RESET_TIMEOUT_MS;
    if (state == BSP_WIFI_STATE_JOINING_AP) return BSP_WIFI_JOIN_TIMEOUT_MS;

    if (bspWifiIsConnectionState(state)) return BSP_WIFI_COMMAND_TIMEOUT_MS;

    return 0U;
}

static void bspWifiEnterState(bsp_wifi_state_t state) {
    s_wifi_context.status.state = state;
    s_wifi_context.command_pending = false;
    s_wifi_context.state_start_ms = otaPlatformMillis();
}

static void bspWifiEnterRetry(void) {
    s_wifi_context.status.wifi_connected = false;
    s_wifi_context.status.retry_count++;
    bspWifiEnterState(BSP_WIFI_STATE_RETRY_WAIT);
}

static void bspWifiHandleDriverEvent(esp8266_event_t event) {
    switch (event) {
        case ESP8266_EVENT_READY:
            s_wifi_context.status.wifi_connected = false;
            bspWifiEnterState(BSP_WIFI_STATE_TESTING);
            break;

        case ESP8266_EVENT_OK:
            switch (s_wifi_context.status.state) {
                case BSP_WIFI_STATE_RESETTING:
                    break;
                case BSP_WIFI_STATE_TESTING:
                    bspWifiEnterState(BSP_WIFI_STATE_DISABLING_ECHO);
                    break;

                case BSP_WIFI_STATE_DISABLING_ECHO:
                    bspWifiEnterState(BSP_WIFI_STATE_SETTING_STATION_MODE);
                    break;

                case BSP_WIFI_STATE_SETTING_STATION_MODE:
                    bspWifiEnterState(BSP_WIFI_STATE_JOINING_AP);
                    break;

                case BSP_WIFI_STATE_JOINING_AP:
                    if (esp8266IsWifiConnected()) {
                        s_wifi_context.status.wifi_connected = true;
                        bspWifiEnterState(BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION);
                    }
                    else {
                        bspWifiEnterRetry();
                    }
                    break;

                case BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION:
                    bspWifiEnterState(BSP_WIFI_STATE_NETWORK_READY);
                    break;

                default:
                    break;
            }
            break;

        case ESP8266_EVENT_WIFI_CONNECTED:
            break;

        case ESP8266_EVENT_WIFI_GOT_IP:
            s_wifi_context.status.wifi_connected = true;
            break;

        case ESP8266_EVENT_WIFI_DISCONNECTED:
            s_wifi_context.status.wifi_connected = false;

            if (s_wifi_context.status.state != BSP_WIFI_STATE_JOINING_AP) bspWifiEnterRetry();
            break;

        case ESP8266_EVENT_ERROR:
        case ESP8266_EVENT_BUSY:
            if (bspWifiIsConnectionState(s_wifi_context.status.state)) bspWifiEnterRetry();
            break;

        case ESP8266_EVENT_PARSE_ERROR:
        case ESP8266_EVENT_NONE:
        default:
            break;
    }
}

static bool bspWifiStartStateCommand(void) {
    bool result;

    result = false;

    switch (s_wifi_context.status.state) {
        case BSP_WIFI_STATE_RESETTING:
            result = esp8266Reset();
            break;

        case BSP_WIFI_STATE_TESTING:
            result = esp8266Test();
            break;

        case BSP_WIFI_STATE_DISABLING_ECHO:
            result = esp8266SetEcho(false);
            break;

        case BSP_WIFI_STATE_SETTING_STATION_MODE:
            result = esp8266SetStationMode();
            break;

        case BSP_WIFI_STATE_JOINING_AP:
            result = esp8266JoinAccessPoint(s_wifi_context.ssid, s_wifi_context.password);
            break;

        case BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION:
            result = esp8266SetSingleConnectionMode();
            break;

        default:
            return false;
    }

    if (result) {
        s_wifi_context.command_pending = true;
        s_wifi_context.state_start_ms = otaPlatformMillis();
    }

    return result;
}

static void bspWifiProcessConnectionState(void) {
    uint32_t timeout_ms;
    uint32_t now;

    if (!bspWifiIsConnectionState(s_wifi_context.status.state)) return;

    if (!s_wifi_context.command_pending) {
        if (!bspWifiStartStateCommand()) {
            bspWifiEnterRetry();
            return;
        }
    }

    timeout_ms = bspWifiGetStateTimeout(s_wifi_context.status.state);

    if (timeout_ms == 0U) return;

    now = otaPlatformMillis();

    if ((now - s_wifi_context.state_start_ms) >= timeout_ms) bspWifiEnterRetry();
}

static void bspWifiProcessRetry(void) {
    if (s_wifi_context.status.state != BSP_WIFI_STATE_RETRY_WAIT) return;

    if ((otaPlatformMillis() - s_wifi_context.state_start_ms) < BSP_WIFI_RETRY_DELAY_MS) return;

    bspWifiEnterState(BSP_WIFI_STATE_RESETTING);
}

// functions
bool bspWifiInit(void) {
    memset(&s_wifi_context, 0, sizeof(s_wifi_context));

    s_wifi_context.status.state = BSP_WIFI_STATE_UNINITIALIZED;

    otaPlatformUartInit();

    if (!esp8266Init()) {
        s_wifi_context.status.state = BSP_WIFI_STATE_FAULT;
        return false;
    }

    esp8266SetEventCallback(bspWifiHandleDriverEvent);

    s_wifi_context.status.initialized = true;
    s_wifi_context.status.state = BSP_WIFI_STATE_IDLE;

    return true;
}

bool bspWifiConfigure(const char *ssid, const char *password) {
    if (!s_wifi_context.status.initialized) return false;

    if (!bspWifiCopyString(s_wifi_context.ssid, sizeof(s_wifi_context.ssid), ssid)) return false;

    if (!bspWifiCopyString(s_wifi_context.password, sizeof(s_wifi_context.password), password)) return false;

    s_wifi_context.status.configured = true;

    return true;
}

bool bspWifiStart(void) {
    if ((!s_wifi_context.status.initialized) || (!s_wifi_context.status.configured) ||
        (s_wifi_context.status.state == BSP_WIFI_STATE_FAULT)) {
        return false;
    }

    s_wifi_context.status.started = true;
    s_wifi_context.status.wifi_connected = false;

    bspWifiEnterState(BSP_WIFI_STATE_RESETTING);

    return true;
}

void bspWifiProcess(void) {
    if (!s_wifi_context.status.initialized) return;

    esp8266Process();

    s_wifi_context.status.rx_overflow_count = esp8266GetRxOverflowCount();

    if (!s_wifi_context.status.started) return;
    if ((s_wifi_context.status.state == BSP_WIFI_STATE_SETTING_SINGLE_CONNECTION) && (!esp8266IsWifiConnected())) bspWifiEnterRetry();

    bspWifiProcessConnectionState();
    bspWifiProcessRetry();
}

bool bspWifiIsConnected(void) { 
    return s_wifi_context.status.wifi_connected && esp8266IsWifiConnected(); 
}

bool bspWifiIsNetworkReady(void) {
    return s_wifi_context.status.initialized && s_wifi_context.status.wifi_connected && esp8266IsWifiConnected() &&
           (s_wifi_context.status.state == BSP_WIFI_STATE_NETWORK_READY);
}

bsp_wifi_status_t bspWifiGetStatus(void) { 
    return s_wifi_context.status; 
}
