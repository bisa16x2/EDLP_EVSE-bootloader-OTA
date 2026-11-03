#ifndef APP_DRIVER__ESP8266_H
#define APP_DRIVER__ESP8266_H

#include <stdbool.h>
#include <stdint.h>

#define ESP8266_REMOTE_HOST_MAX_LENGTH  48U
#define ESP8266_UDP_RX_MAX_LENGTH       2048U
#define ESP8266_UDP_TX_MAX_LENGTH       2048U

// tables
typedef enum {
    ESP8266_EVENT_NONE = 0,
    ESP8266_EVENT_READY,
    ESP8266_EVENT_OK,
    ESP8266_EVENT_ERROR,
    ESP8266_EVENT_BUSY,

    ESP8266_EVENT_WIFI_CONNECTED,
    ESP8266_EVENT_WIFI_GOT_IP,
    ESP8266_EVENT_WIFI_DISCONNECTED,

    ESP8266_EVENT_UDP_OPENED,
    ESP8266_EVENT_UDP_ALREADY_OPEN,
    ESP8266_EVENT_UDP_CLOSED,
    ESP8266_EVENT_UDP_OPEN_FAILED,

    ESP8266_EVENT_JOIN_FAILED,
    ESP8266_EVENT_PARSE_ERROR
} esp8266_event_t;

typedef enum {
    ESP8266_UDP_TX_IDLE = 0,

    ESP8266_UDP_TX_WAIT_PROMPT,

    ESP8266_UDP_TX_WAIT_RESULT,

    ESP8266_UDP_TX_SUCCESS,
    ESP8266_UDP_TX_FAILED
} esp8266_udp_tx_state_t;

typedef void (*esp8266_event_callback_t)(esp8266_event_t event);
typedef void (*esp8266_udp_rx_callback_t)(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);

// functions
bool esp8266Init(void);
void esp8266Process(void);

void esp8266SetEventCallback(esp8266_event_callback_t callback);
void esp8266SetUdpRxCallback(esp8266_udp_rx_callback_t callback);

bool esp8266Test(void);
bool esp8266Reset(void);
bool esp8266SetEcho(bool enable);
bool esp8266SetStationMode(void);
bool esp8266JoinAccessPoint(const char *ssid, const char *password);
uint8_t esp8266GetJoinError(void);

bool esp8266IsWifiConnected(void);
uint32_t esp8266GetRxOverflowCount(void);

bool esp8266SetSingleConnectionMode(void);

// UDP
bool esp8266SetIpdInfo(bool enable);
bool esp8266IsIpdInfoEnabled(void);
bool esp8266OpenUdp(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port);
bool esp8266CloseUdp(void);
bool esp8266IsUdpOpen(void);
bool esp8266SendUdp(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port);
esp8266_udp_tx_state_t esp8266GetUdpTxState(void);
bool esp8266IsUdpTxBusy(void);

#endif // APP_DRIVER__ESP8266_H
