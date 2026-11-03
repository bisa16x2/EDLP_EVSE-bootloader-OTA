#include "esp8266.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_uart.h"

#include "OTA_WIFI/COMMON/ring_buffer.h"

#define ESP8266_UART_CHANNEL            OTA_PLATFORM_UART_WIFI
#define ESP8266_RX_RING_BUFFER_SIZE     4096U
#define ESP8266_LINE_BUFFER_SIZE        192U
#define ESP8266_COMMAND_BUFFER_SIZE     192U
#define ESP8266_IPD_HEADER_BUFFER_SIZE  96U

// tables
typedef enum {
    ESP8266_RX_MODE_LINE = 0,
    ESP8266_RX_MODE_IPD_HEADER,
    ESP8266_RX_MODE_IPD_DATA,
    ESP8266_RX_MODE_IPD_DISCARD
} esp8266_rx_mode_t;

// inners
static ring_buffer_t s_rx_ring;
static uint8_t s_rx_storage[ESP8266_RX_RING_BUFFER_SIZE];
static volatile uint32_t s_rx_overflow_count;

static char s_line_buffer[ESP8266_LINE_BUFFER_SIZE];
static uint16_t s_line_length;

static esp8266_rx_mode_t s_rx_mode;
static char s_ipd_header_buffer[ESP8266_IPD_HEADER_BUFFER_SIZE];
static uint16_t s_ipd_header_length;
static uint8_t s_ipd_payload[ESP8266_UDP_RX_MAX_LENGTH];
static uint16_t s_ipd_expected_length;
static uint16_t s_ipd_received_length;
static uint16_t s_ipd_discard_remaining;
static char s_ipd_remote_host[ESP8266_REMOTE_HOST_MAX_LENGTH];
static uint16_t s_ipd_remote_port;

static uint8_t s_udp_tx_buffer[ESP8266_UDP_TX_MAX_LENGTH];
static uint16_t s_udp_tx_length;
static esp8266_udp_tx_state_t s_udp_tx_state;

static bool s_ipd_info_enabled;
static bool s_ipd_info_command_pending;
static bool s_ipd_info_requested_enable;

static bool s_wifi_connected;
static bool s_udp_open;
static bool s_process_yield_requested;
static uint8_t s_join_error;

static esp8266_event_callback_t s_event_callback;
static esp8266_udp_rx_callback_t s_udp_rx_callback;

static void esp8266EmitEvent(esp8266_event_t event) {
    s_process_yield_requested = true;

    if (s_event_callback != NULL) s_event_callback(event);
}

static void esp8266ResetIpdParser(void) {
    s_rx_mode = ESP8266_RX_MODE_LINE;
    s_ipd_header_length = 0U;
    s_ipd_header_buffer[0] = '\0';
    s_ipd_expected_length = 0U;
    s_ipd_received_length = 0U;
    s_ipd_discard_remaining = 0U;
    s_ipd_remote_host[0] = '\0';
    s_ipd_remote_port = 0U;
}

static void esp8266ResetUdpTx(void) {
    s_udp_tx_length = 0U;
    s_udp_tx_state = ESP8266_UDP_TX_IDLE;
}

static void esp8266ResetConnectionState(void) {
    s_udp_open = false;

    esp8266ResetUdpTx();
}

static char *esp8266TrimString(char *string) {
    char *end;

    if (string == NULL) return NULL;

    while ((*string == ' ') || (*string == '\t') || (*string == '\r') || (*string == '\n')) string++;

    end = string + strlen(string);

    while (end > string) {
        const char character = *(end - 1);

        if ((character != ' ') && (character != '\t') && (character != '\r') && (character != '\n')) break;

        end--;
    }

    *end = '\0';

    return string;
}

static bool esp8266WriteCommand(const char *command) {
    char frame[ESP8266_COMMAND_BUFFER_SIZE];
    int length;

    if ((command == NULL) || (command[0] == '\0')) return false;

    length = snprintf(frame, sizeof(frame), "%s\r\n", command);

    if ((length <= 0) || ((uint32_t)length >= sizeof(frame))) return false;

    return otaPlatformUartWrite(ESP8266_UART_CHANNEL, (const uint8_t *)frame, (uint32_t)length) == (uint32_t)length;
}

static void esp8266HandleConnected(void) {
    s_udp_open = true;
    esp8266EmitEvent(ESP8266_EVENT_UDP_OPENED);
}

static void esp8266HandleAlreadyConnected(void) {
    s_udp_open = true;
    esp8266EmitEvent(ESP8266_EVENT_UDP_ALREADY_OPEN);
}

static void esp8266HandleClosed(void) {
    esp8266ResetConnectionState();
    esp8266EmitEvent(ESP8266_EVENT_UDP_CLOSED);
}

static void esp8266HandleConnectFailed(void) {
    esp8266ResetConnectionState();
    esp8266EmitEvent(ESP8266_EVENT_UDP_OPEN_FAILED);
}

static void esp8266ParseLine(char *line) {
    char *text;

    text = esp8266TrimString(line);

    if ((text == NULL) || (text[0] == '\0')) return;

    (void)otaPlatformUartPrintf(OTA_PLATFORM_UART_DEBUG, "[ESP RX] %s\r\n", text);

    if (strcmp(text, "ready") == 0) {
        s_wifi_connected = false;

        s_ipd_info_enabled = false;
        s_ipd_info_command_pending = false;
        s_ipd_info_requested_enable = false;

        esp8266ResetConnectionState();
        esp8266ResetIpdParser();

        esp8266EmitEvent(ESP8266_EVENT_READY);

        return;
    }

    if (strcmp(text, "OK") == 0) {
        if (s_ipd_info_command_pending) {
            s_ipd_info_enabled = s_ipd_info_requested_enable;
            s_ipd_info_command_pending = false;
        }

        esp8266EmitEvent(ESP8266_EVENT_OK);
        return;
    }

    if (strcmp(text, "SEND OK") == 0) {
        if (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT) {

            s_udp_tx_length = 0U;
            s_udp_tx_state = ESP8266_UDP_TX_SUCCESS;
        }

        return;
    }

    if (strcmp(text, "SEND FAIL") == 0) {
        if ((s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT) || (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT)) {

            s_udp_tx_length = 0U;
            s_udp_tx_state = ESP8266_UDP_TX_FAILED;
        }

        return;
    }

    if (strncmp(text, "busy", 4U) == 0) {
        if ((s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT) || (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT)) {

            s_udp_tx_length = 0U;
            s_udp_tx_state = ESP8266_UDP_TX_FAILED;
        }

        esp8266EmitEvent(ESP8266_EVENT_BUSY);
        return;
    }

    if (strcmp(text, "WIFI CONNECTED") == 0) {
        esp8266EmitEvent(ESP8266_EVENT_WIFI_CONNECTED);
        return;
    }

    if (strcmp(text, "WIFI GOT IP") == 0) {
        s_wifi_connected = true;
        esp8266EmitEvent(ESP8266_EVENT_WIFI_GOT_IP);
        return;
    }

    if (strcmp(text, "WIFI DISCONNECT") == 0) {
        s_wifi_connected = false;
        esp8266ResetConnectionState();
        esp8266EmitEvent(ESP8266_EVENT_WIFI_DISCONNECTED);
        return;
    }

    if (strncmp(text, "+CWJAP:", 7U) == 0) {
        unsigned int error_code;

        error_code = 0U;

        if (sscanf(text, "+CWJAP:%u", &error_code) == 1) {
            s_join_error = (uint8_t)error_code;
            esp8266EmitEvent(ESP8266_EVENT_JOIN_FAILED);
        }
        else {
            esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
        }

        return;
    }

    if (strcmp(text, "CONNECT") == 0) {
        esp8266HandleConnected();
        return;
    }

    if (strcmp(text, "ALREADY CONNECTED") == 0) {
        esp8266HandleAlreadyConnected();
        return;
    }

    if ((strcmp(text, "CLOSED") == 0) || (strstr(text, ",CLOSED") != NULL)) {
        esp8266HandleClosed();
        return;
    }

    if (strcmp(text, "CONNECT FAIL") == 0) {
        esp8266HandleConnectFailed();
        return;
    }

    if ((strcmp(text, "ERROR") == 0) || (strcmp(text, "FAIL") == 0) || (strstr(text, "no this fun") != NULL)) {
        if ((s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT) || (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT)) {

            s_udp_tx_length = 0U;
            s_udp_tx_state = ESP8266_UDP_TX_FAILED;
        }

        s_ipd_info_command_pending = false;

        esp8266EmitEvent(ESP8266_EVENT_ERROR);
    }
}

static bool esp8266ParseUnsigned(const char *text, unsigned long *value) {
    char *end;
    unsigned long parsed;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0')) return false;

    parsed = strtoul(text, &end, 10);

    if ((end == text) || (*end != '\0')) return false;

    *value = parsed;
    return true;
}

static bool esp8266ParseIpdHeader(char *header, uint16_t *payload_length, char *remote_host, size_t remote_host_size, uint16_t *remote_port) {
    char *first_comma;
    char *last_comma;
    unsigned long parsed_length;
    unsigned long parsed_port;
    size_t host_length;
    char *host_begin;
    char *host_end;

    if ((header == NULL) || (payload_length == NULL) || (remote_host == NULL) || (remote_host_size == 0U) ||
        (remote_port == NULL)) {
        return false;
    }

    remote_host[0] = '\0';
    *remote_port = 0U;

    first_comma = strchr(header, ',');

    if (first_comma == NULL) {
        if (!esp8266ParseUnsigned(header, &parsed_length)) return false;
        if ((parsed_length == 0UL) || (parsed_length > UINT16_MAX)) return false;

        *payload_length = (uint16_t)parsed_length;
        return true;
    }

    *first_comma = '\0';

    if (!esp8266ParseUnsigned(header, &parsed_length)) return false;
    if ((parsed_length == 0UL) || (parsed_length > UINT16_MAX)) return false;

    last_comma = strrchr(first_comma + 1, ',');

    if ((last_comma == NULL) || (last_comma == (first_comma + 1))) return false;

    *last_comma = '\0';

    if (!esp8266ParseUnsigned(last_comma + 1, &parsed_port)) return false;
    if ((parsed_port == 0UL) || (parsed_port > UINT16_MAX)) return false;

    host_begin = first_comma + 1;
    host_end = host_begin + strlen(host_begin);

    if ((host_end > host_begin) && (*host_begin == '"') && (*(host_end - 1) == '"')) {
        host_begin++;
        host_end--;
    }

    host_length = (size_t)(host_end - host_begin);

    if ((host_length == 0U) || (host_length >= remote_host_size)) return false;

    memcpy(remote_host, host_begin, host_length);
    remote_host[host_length] = '\0';

    *payload_length = (uint16_t)parsed_length;
    *remote_port = (uint16_t)parsed_port;

    return true;
}

static void esp8266BeginIpdPayload(void) {
    uint16_t payload_length;
    uint16_t remote_port;
    char remote_host[ESP8266_REMOTE_HOST_MAX_LENGTH];

    s_ipd_header_buffer[s_ipd_header_length] = '\0';

    if (!esp8266ParseIpdHeader(s_ipd_header_buffer, &payload_length, remote_host, sizeof(remote_host), &remote_port)) {
        esp8266ResetIpdParser();
        esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
        return;
    }

    s_ipd_expected_length = payload_length;
    s_ipd_received_length = 0U;
    s_ipd_remote_port = remote_port;

    if (remote_host[0] != '\0') memcpy(s_ipd_remote_host, remote_host, strlen(remote_host) + 1U);
    else s_ipd_remote_host[0] = '\0';

    if (payload_length > ESP8266_UDP_RX_MAX_LENGTH) {
        s_ipd_discard_remaining = payload_length;
        s_rx_mode = ESP8266_RX_MODE_IPD_DISCARD;
        return;
    }

    s_rx_mode = ESP8266_RX_MODE_IPD_DATA;
}

static void esp8266CompleteIpdPayload(void) {
    if (s_udp_rx_callback != NULL)
        s_udp_rx_callback(s_ipd_payload, s_ipd_received_length, s_ipd_remote_host, s_ipd_remote_port);

    esp8266ResetIpdParser();
}

static void esp8266ProcessLineByte(uint8_t data) {
    if ((data == '>') && (s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT)) {
        // 혹시 Prompt 앞에 남아 있던 Line data가 있다면 폐기
        s_line_length = 0U;
        s_line_buffer[0] = '\0';

        if (otaPlatformUartWrite(ESP8266_UART_CHANNEL, s_udp_tx_buffer, s_udp_tx_length) == s_udp_tx_length) {

            s_udp_tx_state = ESP8266_UDP_TX_WAIT_RESULT;
        }
        else {
            s_udp_tx_length = 0U;
            s_udp_tx_state = ESP8266_UDP_TX_FAILED;
        }

        return;
    }

    if (s_line_length >= (ESP8266_LINE_BUFFER_SIZE - 1U)) {
        s_line_length = 0U;
        s_line_buffer[0] = '\0';
        esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
        return;
    }

    s_line_buffer[s_line_length] = (char)data;
    s_line_length++;

    if ((s_line_length == 5U) &&
        (memcmp(s_line_buffer, "+IPD,", 5U) == 0)) {
        s_line_length = 0U;
        s_line_buffer[0] = '\0';
        s_ipd_header_length = 0U;
        s_ipd_header_buffer[0] = '\0';
        s_rx_mode = ESP8266_RX_MODE_IPD_HEADER;
        return;
    }

    if (data != '\n') return;

    s_line_buffer[s_line_length] = '\0';
    esp8266ParseLine(s_line_buffer);

    s_line_length = 0U;
    s_line_buffer[0] = '\0';
}

static void esp8266ProcessIpdHeaderByte(uint8_t data) {
    if (data == ':') {
        esp8266BeginIpdPayload();
        return;
    }

    if (s_ipd_header_length >= (ESP8266_IPD_HEADER_BUFFER_SIZE - 1U)) {
        esp8266ResetIpdParser();
        esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
        return;
    }

    s_ipd_header_buffer[s_ipd_header_length] = (char)data;
    s_ipd_header_length++;
}

static void esp8266ProcessIpdDataByte(uint8_t data) {
    if (s_ipd_received_length >= s_ipd_expected_length) {
        esp8266ResetIpdParser();
        esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
        return;
    }

    s_ipd_payload[s_ipd_received_length] = data;
    s_ipd_received_length++;

    if (s_ipd_received_length == s_ipd_expected_length) {
        esp8266CompleteIpdPayload();
    }
}

static void esp8266ProcessIpdDiscardByte(void) {
    if (s_ipd_discard_remaining == 0U) {
        esp8266ResetIpdParser();
        return;
    }

    s_ipd_discard_remaining--;

    if (s_ipd_discard_remaining == 0U) {
        esp8266ResetIpdParser();
        esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
    }
}

static void esp8266ProcessByte(uint8_t data) {
    switch (s_rx_mode) {
        case ESP8266_RX_MODE_LINE:
            esp8266ProcessLineByte(data);
            break;

        case ESP8266_RX_MODE_IPD_HEADER:
            esp8266ProcessIpdHeaderByte(data);
            break;

        case ESP8266_RX_MODE_IPD_DATA:
            esp8266ProcessIpdDataByte(data);
            break;

        case ESP8266_RX_MODE_IPD_DISCARD:
            esp8266ProcessIpdDiscardByte();
            break;

        default:
            esp8266ResetIpdParser();
            esp8266EmitEvent(ESP8266_EVENT_PARSE_ERROR);
            break;
    }
}

static void esp8266UartRxCallback(ota_platform_uart_ch_t ch, uint8_t data) {
    if (ch != ESP8266_UART_CHANNEL) return;

    if (!ringBufferPut(&s_rx_ring, data)) s_rx_overflow_count++;
}

// functions
bool esp8266Init(void) {
    if (!ringBufferInit(&s_rx_ring, s_rx_storage, (uint16_t)sizeof(s_rx_storage))) return false;

    s_rx_overflow_count = 0U;
    s_line_length = 0U;
    s_line_buffer[0] = '\0';

    s_wifi_connected = false;

    s_join_error = 0U;
    s_process_yield_requested = false;

    s_event_callback = NULL;
    s_udp_rx_callback = NULL;

    s_ipd_info_enabled = false;
    s_ipd_info_command_pending = false;
    s_ipd_info_requested_enable = false;

    esp8266ResetUdpTx();

    esp8266ResetConnectionState();
    esp8266ResetIpdParser();

    return otaPlatformUartSetRxCallback(ESP8266_UART_CHANNEL, esp8266UartRxCallback);
}

void esp8266Process(void) {
    uint8_t data;

    s_process_yield_requested = false;

    while ((!s_process_yield_requested) && ringBufferGet(&s_rx_ring, &data)) esp8266ProcessByte(data);
}

void esp8266SetEventCallback(esp8266_event_callback_t callback) { 
    s_event_callback = callback; 
}

void esp8266SetUdpRxCallback(esp8266_udp_rx_callback_t callback) {
    s_udp_rx_callback = callback;
}

bool esp8266Test(void) { 
    return esp8266WriteCommand("AT"); 
}

bool esp8266Reset(void) {
    s_wifi_connected = false;

    s_ipd_info_enabled = false;
    s_ipd_info_command_pending = false;

    esp8266ResetConnectionState();
    esp8266ResetIpdParser();

    return esp8266WriteCommand("AT+RST");
}

bool esp8266SetEcho(bool enable) { 
    return esp8266WriteCommand(enable ? "ATE1" : "ATE0"); 
}

bool esp8266SetStationMode(void) { 
    return esp8266WriteCommand("AT+CWMODE=1"); 
}

bool esp8266JoinAccessPoint(const char *ssid, const char *password) {
    char command[ESP8266_COMMAND_BUFFER_SIZE];
    int length;

    if ((ssid == NULL) || (password == NULL) || (ssid[0] == '\0')) return false;

    length = snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);

    if ((length <= 0) || ((uint32_t)length >= sizeof(command))) return false;

    s_wifi_connected = false;

    return esp8266WriteCommand(command);
}

uint8_t esp8266GetJoinError(void){
    return s_join_error;
}

bool esp8266IsWifiConnected(void) { 
    return s_wifi_connected; 
}

uint32_t esp8266GetRxOverflowCount(void) { 
    return s_rx_overflow_count; 
}

bool esp8266SetSingleConnectionMode(void) { 
    return esp8266WriteCommand("AT+CIPMUX=0"); 
}

bool esp8266SetIpdInfo(bool enable) {
    bool result;

    if (s_ipd_info_command_pending) return false;

    s_ipd_info_requested_enable = enable;
    s_ipd_info_command_pending = true;

    result = esp8266WriteCommand(enable ? "AT+CIPDINFO=1" : "AT+CIPDINFO=0");

    if (!result) s_ipd_info_command_pending = false;

    return result;
}

bool esp8266IsIpdInfoEnabled(void) {
    return s_ipd_info_enabled && (!s_ipd_info_command_pending);
}

bool esp8266OpenUdp(const char *initial_remote_host, uint16_t initial_remote_port, uint16_t local_port) {
    char command[ESP8266_COMMAND_BUFFER_SIZE];
    int length;

    if ((initial_remote_host == NULL) || (initial_remote_host[0] == '\0') || (initial_remote_port == 0U) ||
        (local_port == 0U)) {
        return false;
    }

    length = snprintf(command,
                      sizeof(command),
                      "AT+CIPSTART=\"UDP\",\"%s\",%u,%u,2",
                      initial_remote_host,
                      (unsigned int)initial_remote_port,
                      (unsigned int)local_port);

    if ((length <= 0) || ((uint32_t)length >= sizeof(command))) return false;

    s_udp_open = false;

    return esp8266WriteCommand(command);
}

bool esp8266CloseUdp(void) {
    if (!s_udp_open) return true;
    return esp8266WriteCommand("AT+CIPCLOSE");
}

bool esp8266IsUdpOpen(void) { 
    return s_udp_open; 
}

bool esp8266SendUdp(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port) {

    char command[ESP8266_COMMAND_BUFFER_SIZE];
    int command_length;

    if ((!s_udp_open) || (data == NULL) || (length == 0U) || (length > ESP8266_UDP_TX_MAX_LENGTH) ||
        (remote_host == NULL) || (remote_host[0] == '\0') || (remote_port == 0U)) {
        return false;
    }

    if ((s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT) || (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT)) return false;

    memcpy(s_udp_tx_buffer, data, length);

    s_udp_tx_length = length;

    command_length = snprintf(command,
                              sizeof(command),
                              "AT+CIPSEND=%u,\"%s\",%u",
                              (unsigned int)length,
                              remote_host,
                              (unsigned int)remote_port);

    if ((command_length <= 0) || ((uint32_t)command_length >= sizeof(command))) {

        s_udp_tx_length = 0U;
        s_udp_tx_state = ESP8266_UDP_TX_FAILED;

        return false;
    }

    s_udp_tx_state = ESP8266_UDP_TX_WAIT_PROMPT;

    if (!esp8266WriteCommand(command)) {
        s_udp_tx_length = 0U;
        s_udp_tx_state = ESP8266_UDP_TX_FAILED;

        return false;
    }

    return true;
}

esp8266_udp_tx_state_t esp8266GetUdpTxState(void) {
    return s_udp_tx_state;
}

bool esp8266IsUdpTxBusy(void) {
    return (s_udp_tx_state == ESP8266_UDP_TX_WAIT_PROMPT) || (s_udp_tx_state == ESP8266_UDP_TX_WAIT_RESULT);
}