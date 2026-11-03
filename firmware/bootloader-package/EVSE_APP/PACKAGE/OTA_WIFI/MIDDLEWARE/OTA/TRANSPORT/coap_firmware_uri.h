#ifndef OTA_TRANSPORT__COAP_FIRMWARE_URI_H
#define OTA_TRANSPORT__COAP_FIRMWARE_URI_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    COAP_FIRMWARE_URI_STATUS_OK = 0,

    COAP_FIRMWARE_URI_STATUS_INVALID,
    COAP_FIRMWARE_URI_STATUS_UNSUPPORTED_SCHEME
} coap_firmware_uri_status_t;

typedef struct {
    const char *host;
    size_t host_length;

    uint16_t port;

    const char *path;
    size_t path_length;

    const char *query;
    size_t query_length;
} coap_firmware_uri_t;

coap_firmware_uri_status_t coapFirmwareUriParse(const char *uri, size_t uri_length, coap_firmware_uri_t *parsed_uri);

#endif // OTA_TRANSPORT__COAP_FIRMWARE_URI_H