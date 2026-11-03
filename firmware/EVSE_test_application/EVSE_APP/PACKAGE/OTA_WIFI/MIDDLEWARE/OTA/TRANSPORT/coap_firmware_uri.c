#include "coap_firmware_uri.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define COAP_FIRMWARE_DEFAULT_PORT 5683U

static bool coapFirmwareUriFindSchemeLength(const char *uri, size_t uri_length, size_t *scheme_length) {
    size_t index;

    if ((uri == NULL) || (scheme_length == NULL)) return false;

    for (index = 0U; (index + 2U) < uri_length; index++) {
        if ((uri[index] == ':') && (uri[index + 1U] == '/') && (uri[index + 2U] == '/')) {
            *scheme_length = index;

            return true;
        }
    }

    return false;
}

static bool coapFirmwareUriParsePort(const char *text, size_t text_length, uint16_t *port) {
    uint32_t value = 0U;
    size_t index;

    if ((text == NULL) || (port == NULL) || (text_length == 0U)) return false;

    for (index = 0U; index < text_length; index++) {
        if ((text[index] < '0') || (text[index] > '9')) return false;

        value = (value * 10U) + (uint32_t)(text[index] - '0');

        if (value > UINT16_MAX) return false;
    }

    if (value == 0U) return false;

    *port = (uint16_t)value;

    return true;
}

coap_firmware_uri_status_t coapFirmwareUriParse(const char *uri, size_t uri_length, coap_firmware_uri_t *parsed_uri) {

    size_t scheme_length;

    size_t authority_start;
    size_t authority_end;

    size_t colon_index;
    size_t cursor;
    size_t index;

    if ((uri == NULL) || (uri_length == 0U) || (parsed_uri == NULL)) return COAP_FIRMWARE_URI_STATUS_INVALID;

    memset(parsed_uri, 0, sizeof(*parsed_uri));

    if (!coapFirmwareUriFindSchemeLength(uri, uri_length, &scheme_length)) return COAP_FIRMWARE_URI_STATUS_INVALID;

    if (scheme_length == 0U) return COAP_FIRMWARE_URI_STATUS_INVALID;

    if ((scheme_length != (sizeof("coap") - 1U)) || (memcmp(uri, "coap", sizeof("coap") - 1U) != 0))
        return COAP_FIRMWARE_URI_STATUS_UNSUPPORTED_SCHEME;

    authority_start = scheme_length + 3U;
    authority_end = authority_start;

    while ((authority_end < uri_length) && (uri[authority_end] != '/') && (uri[authority_end] != '?') &&
           (uri[authority_end] != '#')) {

        authority_end++;
    }

    if (authority_end == authority_start) return COAP_FIRMWARE_URI_STATUS_INVALID;

    colon_index = authority_end;

    for (index = authority_start; index < authority_end; index++) {
        if (uri[index] == ':') {
            if (colon_index != authority_end) return COAP_FIRMWARE_URI_STATUS_INVALID;

            colon_index = index;
        }
    }

    if (colon_index == authority_start) return COAP_FIRMWARE_URI_STATUS_INVALID;

    parsed_uri->host = uri + authority_start;
    parsed_uri->host_length = colon_index - authority_start;
    parsed_uri->port = COAP_FIRMWARE_DEFAULT_PORT;

    if (colon_index < authority_end) {
        if (!coapFirmwareUriParsePort(uri + colon_index + 1U, authority_end - colon_index - 1U, &parsed_uri->port))
            return COAP_FIRMWARE_URI_STATUS_INVALID;
    }

    cursor = authority_end;

    if ((cursor < uri_length) && (uri[cursor] == '/')) {
        size_t path_start;

        cursor++;
        path_start = cursor;

        while ((cursor < uri_length) && (uri[cursor] != '?') && (uri[cursor] != '#')) cursor++;

        parsed_uri->path = uri + path_start;
        parsed_uri->path_length = cursor - path_start;
    }

    if ((cursor < uri_length) && (uri[cursor] == '?')) {
        size_t query_start;

        cursor++;
        query_start = cursor;

        while ((cursor < uri_length) && (uri[cursor] != '#')) cursor++;

        parsed_uri->query = uri + query_start;
        parsed_uri->query_length = cursor - query_start;
    }

    if (cursor != uri_length) return COAP_FIRMWARE_URI_STATUS_INVALID;

    return COAP_FIRMWARE_URI_STATUS_OK;
}