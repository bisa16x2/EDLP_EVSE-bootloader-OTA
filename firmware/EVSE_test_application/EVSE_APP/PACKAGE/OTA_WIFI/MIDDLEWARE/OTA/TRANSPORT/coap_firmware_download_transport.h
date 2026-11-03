#ifndef OTA_TRANSPORT__COAP_FIRMWARE_DOWNLOAD_TRANSPORT_H
#define OTA_TRANSPORT__COAP_FIRMWARE_DOWNLOAD_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_download_transport.h"

// tables
typedef enum {
    COAP_FIRMWARE_DOWNLOAD_STATE_UNINITIALIZED = 0,

    COAP_FIRMWARE_DOWNLOAD_STATE_IDLE,

    COAP_FIRMWARE_DOWNLOAD_STATE_REQUEST_PENDING,

    COAP_FIRMWARE_DOWNLOAD_STATE_WAIT_RESPONSE,

    COAP_FIRMWARE_DOWNLOAD_STATE_RESPONSE_RECEIVED,

    COAP_FIRMWARE_DOWNLOAD_STATE_ERROR
} coap_firmware_download_state_t;

typedef enum {
    COAP_FIRMWARE_DOWNLOAD_ERROR_NONE = 0,

    COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_URI,
    COAP_FIRMWARE_DOWNLOAD_ERROR_UNSUPPORTED_PROTOCOL,
    COAP_FIRMWARE_DOWNLOAD_ERROR_URI_TOO_LONG,

    COAP_FIRMWARE_DOWNLOAD_ERROR_BUSY,
    COAP_FIRMWARE_DOWNLOAD_ERROR_NETWORK_NOT_READY,

    COAP_FIRMWARE_DOWNLOAD_ERROR_PACKET_BUILD,
    COAP_FIRMWARE_DOWNLOAD_ERROR_SEND_FAILED,
    COAP_FIRMWARE_DOWNLOAD_ERROR_RESPONSE_TIMEOUT,
    COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE,

    COAP_FIRMWARE_DOWNLOAD_ERROR_SERVICE,
    COAP_FIRMWARE_DOWNLOAD_ERROR_INTERNAL
} coap_firmware_download_error_t;

typedef struct {
    bool initialized;
    bool active;
    bool interceptor_registered;

    coap_firmware_download_state_t state;
    coap_firmware_download_error_t last_error;

    size_t uri_length;

    uint16_t remote_port;

    uint16_t last_request_mid;
    uint16_t last_response_mid;

    uint8_t last_response_code;
    uint16_t last_response_length;

    uint32_t block_number;
    uint16_t block_size;

    uint32_t total_size;
    uint32_t downloaded_size;

    uint8_t retry_count;

    uint32_t request_count;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t cancel_count;
    uint32_t completed_count;
} coap_firmware_download_status_t;

// functions
bool coapFirmwareDownloadTransportInit(firmware_download_transport_t *transport);

void coapFirmwareDownloadTransportProcess(void);

bool coapFirmwareDownloadTransportIsActive(void);

coap_firmware_download_status_t coapFirmwareDownloadTransportGetStatus(void);

#endif // OTA_TRANSPORT__COAP_FIRMWARE_DOWNLOAD_TRANSPORT_H