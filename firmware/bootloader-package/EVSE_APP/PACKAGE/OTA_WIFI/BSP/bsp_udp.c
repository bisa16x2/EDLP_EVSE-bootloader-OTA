#include "bsp_udp.h"

#include <stddef.h>
#include <string.h>

#include "OTA_WIFI/DRIVER/esp8266.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_time.h"

#define BSP_UDP_HOST_MAX_LENGTH     127U

#define BSP_UDP_IPD_INFO_TIMEOUT_MS 3000U
#define BSP_UDP_OPEN_TIMEOUT_MS     10000U
#define BSP_UDP_CLOSE_TIMEOUT_MS    3000U

// tables
typedef struct {
    bsp_udp_status_t status;

    char initial_remote_host[BSP_UDP_HOST_MAX_LENGTH + 1U];
    uint16_t initial_remote_port;
    uint16_t local_port;

    bool state_command_started;
    uint32_t state_start_ms;

    bsp_udp_rx_callback_t rx_callback;
} bsp_udp_context_t;

// inners
static bsp_udp_context_t s_udp_context;

static esp8266_udp_tx_state_t s_previous_driver_tx_state;

/* Internal utility */
static bool bspUdpCopyString(char *destination, size_t destination_size, const char *source) {

    size_t length;

    if ((destination == NULL) || (source == NULL) || (destination_size == 0U)) return false;

    length = strlen(source);

    if ((length == 0U) || (length >= destination_size)) return false;

    memcpy(destination, source, length + 1U);

    return true;
}

static void bspUdpEnterState(bsp_udp_state_t state) {
    s_udp_context.status.state = state;

    s_udp_context.state_command_started = false;
    s_udp_context.state_start_ms = otaPlatformMillis();
}

static void bspUdpEnterError(bsp_udp_error_t error) {
    s_udp_context.status.last_error = error;

    bspUdpEnterState(BSP_UDP_STATE_ERROR);
}

/* RX */

static void bspUdpHandleDriverRx(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {
    s_udp_context.status.rx_count++;

    if (s_udp_context.rx_callback != NULL) s_udp_context.rx_callback(data, length, remote_host, remote_port);
}

/* TX */
static bsp_udp_tx_state_t bspUdpMapDriverTxState(esp8266_udp_tx_state_t driver_state) {
    switch (driver_state) {
        case ESP8266_UDP_TX_WAIT_PROMPT:
        case ESP8266_UDP_TX_WAIT_RESULT: return BSP_UDP_TX_BUSY;
        case ESP8266_UDP_TX_SUCCESS:     return BSP_UDP_TX_SUCCESS;
        case ESP8266_UDP_TX_FAILED:      return BSP_UDP_TX_FAILED;
        case ESP8266_UDP_TX_IDLE:
        default:                         return BSP_UDP_TX_IDLE;
    }
}

static void bspUdpUpdateTxState(void) {
    esp8266_udp_tx_state_t driver_state;

    driver_state = esp8266GetUdpTxState();

    s_udp_context.status.tx_state = bspUdpMapDriverTxState(driver_state);

    if (driver_state == s_previous_driver_tx_state) return;

    if (driver_state == ESP8266_UDP_TX_SUCCESS) s_udp_context.status.tx_success_count++;
    else if (driver_state == ESP8266_UDP_TX_FAILED) s_udp_context.status.tx_fail_count++;

    s_previous_driver_tx_state = driver_state;
}

/* UDP open state machine */
static void bspUdpProcessEnableIpdInfo(void) {
    uint32_t now;

    if (!esp8266IsWifiConnected()) {
        bspUdpEnterError(BSP_UDP_ERROR_CONNECTION_LOST);
        return;
    }

    if (esp8266IsIpdInfoEnabled()) {
        bspUdpEnterState(BSP_UDP_STATE_OPENING);
        return;
    }

    if (!s_udp_context.state_command_started) {
        if (!esp8266SetIpdInfo(true)) {
            bspUdpEnterError(BSP_UDP_ERROR_COMMAND_FAILED);
            return;
        }

        s_udp_context.state_command_started = true;
        s_udp_context.state_start_ms = otaPlatformMillis();

        return;
    }

    now = otaPlatformMillis();

    if ((now - s_udp_context.state_start_ms) >= BSP_UDP_IPD_INFO_TIMEOUT_MS) bspUdpEnterError(BSP_UDP_ERROR_TIMEOUT);
}

static void bspUdpProcessOpening(void) {
    uint32_t now;

    if (!esp8266IsWifiConnected()) {
        bspUdpEnterError(BSP_UDP_ERROR_CONNECTION_LOST);
        return;
    }

    if (esp8266IsUdpOpen()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_NONE;
        bspUdpEnterState(BSP_UDP_STATE_READY);

        return;
    }

    if (!s_udp_context.state_command_started) {
        if (!esp8266OpenUdp(s_udp_context.initial_remote_host,
                            s_udp_context.initial_remote_port,
                            s_udp_context.local_port)) {

            bspUdpEnterError(BSP_UDP_ERROR_COMMAND_FAILED);

            return;
        }

        s_udp_context.state_command_started = true;
        s_udp_context.state_start_ms = otaPlatformMillis();

        return;
    }

    now = otaPlatformMillis();

    if ((now - s_udp_context.state_start_ms) >= BSP_UDP_OPEN_TIMEOUT_MS) bspUdpEnterError(BSP_UDP_ERROR_TIMEOUT);
}

static void bspUdpProcessReady(void) {
    if (!esp8266IsWifiConnected()) {
        bspUdpEnterError(BSP_UDP_ERROR_CONNECTION_LOST);
        return;
    }

    if (!esp8266IsUdpOpen()) {
        bspUdpEnterError(BSP_UDP_ERROR_CONNECTION_LOST);
        return;
    }
}

static void bspUdpProcessClosing(void) {
    uint32_t now;

    if (!esp8266IsUdpOpen()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_NONE;
        bspUdpEnterState(BSP_UDP_STATE_IDLE);

        return;
    }

    if (!s_udp_context.state_command_started) {
        if (!esp8266CloseUdp()) {
            bspUdpEnterError(BSP_UDP_ERROR_COMMAND_FAILED);
            return;
        }

        s_udp_context.state_command_started = true;
        s_udp_context.state_start_ms = otaPlatformMillis();

        return;
    }

    now = otaPlatformMillis();

    if ((now - s_udp_context.state_start_ms) >= BSP_UDP_CLOSE_TIMEOUT_MS) bspUdpEnterError(BSP_UDP_ERROR_TIMEOUT);
}

// functions
bool bspUdpInit(void) {
    memset(&s_udp_context, 0, sizeof(s_udp_context));

    s_udp_context.status.state = BSP_UDP_STATE_UNINITIALIZED;

    esp8266SetUdpRxCallback(bspUdpHandleDriverRx);

    s_previous_driver_tx_state = esp8266GetUdpTxState();

    s_udp_context.status.tx_state = bspUdpMapDriverTxState(s_previous_driver_tx_state);
    s_udp_context.status.initialized = true;
    s_udp_context.status.last_error = BSP_UDP_ERROR_NONE;

    bspUdpEnterState(BSP_UDP_STATE_IDLE);

    return true;
}

bool bspUdpSetRxCallback(bsp_udp_rx_callback_t callback) {
    if (!s_udp_context.status.initialized) return false;

    s_udp_context.rx_callback = callback;

    return true;
}

bool bspUdpOpen(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port) {

    if (!s_udp_context.status.initialized) return false;

    if ((initial_remote_host == NULL) || (initial_remote_host[0] == '\0') || (initial_remote_port == 0U) ||
        (local_port == 0U)) {

        s_udp_context.status.last_error = BSP_UDP_ERROR_INVALID_ARGUMENT;

        return false;
    }

    if (!esp8266IsWifiConnected()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_NETWORK_NOT_READY;

        return false;
    }

    if ((s_udp_context.status.state == BSP_UDP_STATE_ENABLING_IPD_INFO) ||
        (s_udp_context.status.state == BSP_UDP_STATE_OPENING) ||
        (s_udp_context.status.state == BSP_UDP_STATE_CLOSING) || (s_udp_context.status.state == BSP_UDP_STATE_READY)) {

        return false;
    }

    if (esp8266IsUdpOpen()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_SOCKET_ALREADY_OPEN;

        return false;
    }

    if (!bspUdpCopyString(s_udp_context.initial_remote_host,
                          sizeof(s_udp_context.initial_remote_host),
                          initial_remote_host)) {

        s_udp_context.status.last_error = BSP_UDP_ERROR_INVALID_ARGUMENT;

        return false;
    }

    s_udp_context.initial_remote_port = initial_remote_port;
    s_udp_context.local_port = local_port;
    s_udp_context.status.last_error = BSP_UDP_ERROR_NONE;

    bspUdpEnterState(BSP_UDP_STATE_ENABLING_IPD_INFO);

    return true;
}

bool bspUdpClose(void) {
    if (!s_udp_context.status.initialized) return false;
    if (s_udp_context.status.state == BSP_UDP_STATE_CLOSING) return true;
    if ((s_udp_context.status.state == BSP_UDP_STATE_ENABLING_IPD_INFO) ||
        (s_udp_context.status.state == BSP_UDP_STATE_OPENING)) {

        return false;
    }

    if (!esp8266IsUdpOpen()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_NONE;

        bspUdpEnterState(BSP_UDP_STATE_IDLE);

        return true;
    }

    bspUdpEnterState(BSP_UDP_STATE_CLOSING);

    return true;
}

bool bspUdpSendTo(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {

    if ((!s_udp_context.status.initialized) || (data == NULL) || (length == 0U) || (remote_host == NULL) ||
        (remote_host[0] == '\0') || (remote_port == 0U)) {

        s_udp_context.status.last_error = BSP_UDP_ERROR_INVALID_ARGUMENT;

        return false;
    }

    if (!bspUdpIsReady()) {
        s_udp_context.status.last_error = BSP_UDP_ERROR_NOT_READY;

        return false;
    }

    if (esp8266IsUdpTxBusy()) return false;

    if (!esp8266SendUdp(data, length, remote_host, remote_port)) {

        s_udp_context.status.last_error = BSP_UDP_ERROR_COMMAND_FAILED;

        return false;
    }

    s_udp_context.status.tx_state = BSP_UDP_TX_BUSY;

    return true;
}

void bspUdpProcess(void) {
    if (!s_udp_context.status.initialized) return;

    bspUdpUpdateTxState();

    switch (s_udp_context.status.state) {
        case BSP_UDP_STATE_ENABLING_IPD_INFO:
            bspUdpProcessEnableIpdInfo();
            break;

        case BSP_UDP_STATE_OPENING:
            bspUdpProcessOpening();
            break;

        case BSP_UDP_STATE_READY:
            bspUdpProcessReady();
            break;

        case BSP_UDP_STATE_CLOSING:
            bspUdpProcessClosing();
            break;

        case BSP_UDP_STATE_UNINITIALIZED:
        case BSP_UDP_STATE_IDLE:
        case BSP_UDP_STATE_ERROR:
        default: break;
    }
}

bool bspUdpIsReady(void) {
    return s_udp_context.status.initialized && (s_udp_context.status.state == BSP_UDP_STATE_READY) &&
           esp8266IsWifiConnected() && esp8266IsUdpOpen();
}

bool bspUdpIsTxBusy(void) {
    if (!s_udp_context.status.initialized) return false;

    return esp8266IsUdpTxBusy();
}

bsp_udp_tx_state_t bspUdpGetTxState(void) {
    if (!s_udp_context.status.initialized) return BSP_UDP_TX_IDLE;

    return bspUdpMapDriverTxState(esp8266GetUdpTxState());
}

bsp_udp_status_t bspUdpGetStatus(void) {
    bsp_udp_status_t status;

    status = s_udp_context.status;
    status.ipd_info_enabled = esp8266IsIpdInfoEnabled();
    status.socket_open = esp8266IsUdpOpen();
    status.tx_state = bspUdpGetTxState();

    return status;
}