#include "ap.h"

#include <stdbool.h>
#include <stdint.h>

#include "cmsis_os2.h"

#include "BSP/bsp.h"
#include "BSP/bsp_led.h"

#include "HW/hw.h"
#include "HW/hw_uart.h"

#include "COMMON/app_config.h"
#include "COMMON/ota_integration_config.h"

#include "OTA_WIFI/TASK/WIFI/wifi_task.h"
#include "OTA_WIFI/TASK/OTA/ota_task.h"
#include "OTA_WIFI/PLATFORM/STM32/ota_platform_uart.h"

// inners
static volatile app_init_state_t s_app_init_state = APP_INIT_STATE_NOT_STARTED;

static bool s_wifi_state_known;
static bool s_previous_wifi_connected;

static const wifi_task_config_t s_wifi_task_config = {
    .ssid = OTA_INTEGRATION_WIFI_SSID,
    .password = OTA_INTEGRATION_WIFI_PASSWORD
};

static const ota_task_config_t s_ota_task_config = {
    .endpoint_prefix = OTA_INTEGRATION_ENDPOINT_PREFIX,

    .server_host = OTA_INTEGRATION_SERVER_HOST,

    .server_port = OTA_INTEGRATION_SERVER_PORT,
    .local_port = OTA_INTEGRATION_LOCAL_PORT,

    .short_server_id = OTA_INTEGRATION_SHORT_SERVER_ID,

    .lifetime_seconds = OTA_INTEGRATION_LIFETIME_SECONDS,
    
    .boot_reset_delay_ms = OTA_INTEGRATION_BOOT_RESET_DELAY_MS
};

static void apPrintStartMessage(void) {
    (void)hwUartPrintf(HW_UART_DEBUG,
                       "[APP] EVSE Application v%u.%u.%u start\r\n",
                       (unsigned int)APP_VERSION_MAJOR,
                       (unsigned int)APP_VERSION_MINOR,
                       (unsigned int)APP_VERSION_PATCH);
}

static void apApplyLedInterval(bool wifi_connected) {
    uint32_t interval_ms;

    interval_ms = wifi_connected ? APP_LED_INTERVAL_CONNECTED_MS : APP_LED_INTERVAL_DISCONNECTED_MS;

    if (bspLedGetBlinkInterval() == interval_ms) return;

    (void)bspLedSetBlinkInterval(interval_ms);

    (void)hwUartPrintf(HW_UART_DEBUG,
                       "[APP] Wi-Fi %s, LED toggle interval=%lu ms\r\n",
                       wifi_connected ? "connected" : "disconnected",
                       (unsigned long)interval_ms);
}

// functions
bool apInit(void) {
    if (s_app_init_state == APP_INIT_STATE_READY) return true;
    if (s_app_init_state == APP_INIT_STATE_INITIALIZING) return false;

    s_app_init_state = APP_INIT_STATE_INITIALIZING;

    // HW 초기화
    if (!hwInit()) {
        s_app_init_state = APP_INIT_STATE_FAULT;
        return false;
    }

    apPrintStartMessage();

    // BSP 초기화
    if (!bspInit()) {
        s_app_init_state = APP_INIT_STATE_FAULT;
        return false;
    }

    (void)hwUartWriteString(HW_UART_DEBUG, "[NET] Datagram ready\r\n");
    
    if (!wifiTaskInit(&s_wifi_task_config)) {
        (void)hwUartWriteString(HW_UART_DEBUG, "[WIFI] WiFi Task initialization failed\r\n");

        s_app_init_state = APP_INIT_STATE_FAULT;

        return false;
    }

    (void)hwUartWriteString(HW_UART_DEBUG, "[WIFI] WiFi Task initialized\r\n");

    if (!otaTaskInit(&s_ota_task_config)) {
        (void)hwUartWriteString(HW_UART_DEBUG, "[OTA] OTA Task initialization failed\r\n");

        s_app_init_state = APP_INIT_STATE_FAULT;

        return false;
    }

    (void)hwUartWriteString(HW_UART_DEBUG, "[OTA] OTA Task initialized\r\n");

    (void)hwUartWriteString(HW_UART_DEBUG, "[OTA] CoAP Download Transport ready\r\n");

    if (!bspLedSetBlinkInterval(APP_LED_INTERVAL_DISCONNECTED_MS)) {
        s_app_init_state = APP_INIT_STATE_FAULT;
        return false;
    }

    s_wifi_state_known = false;
    s_previous_wifi_connected = false;

    s_app_init_state = APP_INIT_STATE_READY;

    return true;
}

app_init_state_t apGetInitState(void) { 
    return s_app_init_state; 
}

void apControlMain(void) {
    bool wifi_connected;

    if (otaTaskIsInstallPending()) {
        if (!otaTaskCommitInstall()) (void)hwUartWriteString(HW_UART_DEBUG, "[OTA] Install commit failed\r\n");
    }

    // 예약된 Bootloader 진입 요청 처리

    // Schedule 후 100ms가 지나면:
    // bspBootSetUpdateRequest()
    // → Backup SRAM Magic 기록
    // → bspBootReset()

    wifi_connected = wifiTaskIsConnected();

    if ((!s_wifi_state_known) || (wifi_connected != s_previous_wifi_connected)) {
        apApplyLedInterval(wifi_connected);

        s_previous_wifi_connected = wifi_connected;
        s_wifi_state_known = true;
    }

    bspLedProcess();

    osDelay(APP_CONTROL_PERIOD_MS);
}

void apWifiMain(void) {
    wifiTaskProcess();

    osDelay(APP_WIFI_TASK_PERIOD_MS);
}

void apOtaMain(void) {
    otaTaskProcess();

    osDelay(APP_OTA_TASK_PERIOD_MS);
}

void StartControlTask(void *argument) {
    (void)argument;

    if (!apInit()) {
        (void)hwUartWriteString(HW_UART_DEBUG, "[APP] Initialization failed\r\n");

        while (1) {
            bspLedProcess();
            osDelay(APP_CONTROL_PERIOD_MS);
        }
    }

    while (1) apControlMain();
}

void StartWifiTask(void *argument) {
    (void)argument;

    while ((apGetInitState() == APP_INIT_STATE_NOT_STARTED) || (apGetInitState() == APP_INIT_STATE_INITIALIZING))
        osDelay(1U);

    while (1)
        if (apGetInitState() == APP_INIT_STATE_READY) apWifiMain();
        else osDelay(100U);
}

void StartOtaTask(void *argument) {
    (void)argument;

    while ((apGetInitState() == APP_INIT_STATE_NOT_STARTED) || (apGetInitState() == APP_INIT_STATE_INITIALIZING))
        osDelay(1U);

    while (1)
        if (apGetInitState() == APP_INIT_STATE_READY) apOtaMain();
        else osDelay(100U);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    otaPlatformUartHandleRxCplt(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    otaPlatformUartHandleError(huart);
}
