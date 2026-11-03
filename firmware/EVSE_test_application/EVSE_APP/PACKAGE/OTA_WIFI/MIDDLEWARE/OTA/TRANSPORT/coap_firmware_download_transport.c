#include "coap_firmware_download_transport.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "coap_firmware_uri.h"
#include "er-coap-13.h"

#include "OTA_WIFI/MIDDLEWARE/LWM2M/wakaama_network_port.h"
#include "OTA_WIFI/MIDDLEWARE/NETWORK/net_datagram.h"

#include "OTA_WIFI/PLATFORM/STM32/ota_platform_time.h"

#define COAP_FIRMWARE_DOWNLOAD_URI_MAX_LENGTH   511U
#define COAP_FIRMWARE_DOWNLOAD_HOST_MAX_LENGTH  127U
#define COAP_FIRMWARE_DOWNLOAD_PATH_MAX_LENGTH  255U
#define COAP_FIRMWARE_DOWNLOAD_QUERY_MAX_LENGTH 255U

#define COAP_FIRMWARE_DOWNLOAD_TX_BUFFER_SIZE   1024U

#define COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE       512U

#define COAP_FIRMWARE_DEFAULT_PORT              5683U

#define COAP_FIRMWARE_TOKEN_LENGTH              4U

#define COAP_FIRMWARE_INITIAL_RETRY_TIMEOUT_MS  2000U
#define COAP_FIRMWARE_MAX_RETRY_TIMEOUT_MS      8000U
#define COAP_FIRMWARE_MAX_RETRY_COUNT           4U

// tables
typedef struct {
    coap_firmware_download_status_t status;

    firmware_download_transport_t *public_transport;
    firmware_update_service_t *service;

    char request_uri[COAP_FIRMWARE_DOWNLOAD_URI_MAX_LENGTH + 1U];

    coap_firmware_uri_t parsed_uri;

    char remote_host[COAP_FIRMWARE_DOWNLOAD_HOST_MAX_LENGTH + 1U];

    char path[COAP_FIRMWARE_DOWNLOAD_PATH_MAX_LENGTH + 1U];

    char query[COAP_FIRMWARE_DOWNLOAD_QUERY_MAX_LENGTH + 1U];

    uint8_t token[COAP_FIRMWARE_TOKEN_LENGTH];

    uint16_t request_mid;
    uint16_t request_sequence;

    uint32_t block_number;
    uint16_t block_size;

    uint32_t total_size;

    uint8_t retry_count;
    uint32_t retry_timeout_ms;
    uint32_t response_deadline_ms;

    uint8_t last_progress_percent;

    uint8_t tx_buffer[COAP_FIRMWARE_DOWNLOAD_TX_BUFFER_SIZE];

    uint16_t tx_length;
} coap_firmware_download_context_t;

// inners
static coap_firmware_download_context_t s_download;

static bool coapFirmwareDownloadCopySlice(char *destination, size_t destination_size, const char *source, size_t source_length) {
    if ((destination == NULL) || (destination_size == 0U)) return false;

    destination[0] = '\0';

    if (source_length == 0U) return true;

    if ((source == NULL) || (source_length >= destination_size)) return false;

    memcpy(destination, source, source_length);

    destination[source_length] = '\0';

    return true;
}

static void coapFirmwareDownloadClearSession(coap_firmware_download_context_t *context) {
    if (context == NULL) return;

    context->service = NULL;

    memset(context->request_uri, 0, sizeof(context->request_uri));
    memset(&context->parsed_uri, 0, sizeof(context->parsed_uri));
    memset(context->remote_host, 0, sizeof(context->remote_host));
    memset(context->path, 0, sizeof(context->path));
    memset(context->query, 0, sizeof(context->query));
    memset(context->token, 0, sizeof(context->token));

    context->request_mid = 0U;

    context->block_number = 0U;
    context->block_size = COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE;

    context->total_size = 0U;

    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_FIRMWARE_INITIAL_RETRY_TIMEOUT_MS;

    context->response_deadline_ms = 0U;

    context->last_progress_percent = 0U;

    context->tx_length = 0U;
}

static firmware_download_transport_status_t coapFirmwareDownloadMapUriStatus(coap_firmware_uri_status_t uri_status) {
    switch (uri_status) {
        case COAP_FIRMWARE_URI_STATUS_OK:                   return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
        case COAP_FIRMWARE_URI_STATUS_INVALID:              return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
        case COAP_FIRMWARE_URI_STATUS_UNSUPPORTED_SCHEME:   return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_UNSUPPORTED_PROTOCOL;
        default:                                            return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }
}

static void coapFirmwareDownloadStopWithError(coap_firmware_download_context_t *context, firmware_update_download_failure_t failure, coap_firmware_download_error_t error) {
    firmware_update_service_t *service;

    if (context == NULL) return;

    service = context->service;

    context->status.active = false;
    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_ERROR;
    context->status.last_error = error;

    context->service = NULL;

    if (service != NULL) (void)firmware_update_service_fail_download(service, failure);
}

static void coapFirmwareDownloadStopForServiceFailure(coap_firmware_download_context_t *context) {
    if (context == NULL) return;

    context->status.active = false;
    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_ERROR;
    context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_SERVICE;

    context->service = NULL;
}

static bool coapFirmwareDownloadDeadlineExpired(uint32_t now, uint32_t deadline) {
    return ((int32_t)(now - deadline) >= 0);
}

// Request identity
static void coapFirmwareDownloadGenerateRequestIdentity(coap_firmware_download_context_t *context) {
    uint16_t sequence;

    context->request_sequence++;

    if (context->request_sequence == 0U) context->request_sequence = 1U;

    sequence = context->request_sequence;

    context->token[0] = 0x46U;
    context->token[1] = 0x57U;
    context->token[2] = (uint8_t)((sequence >> 8U) & 0xFFU);
    context->token[3] = (uint8_t)(sequence & 0xFFU);

    context->request_mid = (uint16_t)(0xC000U | (sequence & 0x3FFFU));

    context->status.last_request_mid = context->request_mid;
}

// CoAP Request
static bool coapFirmwareDownloadPrepareRequest(coap_firmware_download_context_t *context) {
    coap_packet_t request;

    size_t required_size;
    size_t serialized_length;

    if (context == NULL) return false;

    coapFirmwareDownloadGenerateRequestIdentity(context);

    memset(&request, 0, sizeof(request));

    memset(context->tx_buffer, 0, sizeof(context->tx_buffer));

    coap_init_message(&request, COAP_TYPE_CON, COAP_GET, context->request_mid);

    if (coap_set_header_token(&request, context->token, COAP_FIRMWARE_TOKEN_LENGTH) != (int)COAP_FIRMWARE_TOKEN_LENGTH) {
        coap_free_header(&request);

        return false;
    }

    if (coap_set_header_uri_host(&request, context->remote_host) <= 0) {
        coap_free_header(&request);

        return false;
    }

    if (context->parsed_uri.port != COAP_FIRMWARE_DEFAULT_PORT)
        coap_set_header_uri_port(&request, context->parsed_uri.port);

    if (context->path[0] != '\0') {
        if (coap_set_header_uri_path(&request, context->path) <= 0) {
            coap_free_header(&request);

            return false;
        }
    }

    if (context->query[0] != '\0') {
        if (coap_set_header_uri_query(&request, context->query) <= 0) {
            coap_free_header(&request);

            return false;
        }
    }

    if (!coap_set_header_block2(&request, context->block_number, 0U, context->block_size)) {
        coap_free_header(&request);

        return false;
    }

    required_size = coap_serialize_get_size(&request);

    if ((required_size == 0U) || (required_size > sizeof(context->tx_buffer))) {
        coap_free_header(&request);

        return false;
    }

    serialized_length = coap_serialize_message(&request, context->tx_buffer);

    coap_free_header(&request);

    if ((serialized_length == 0U) || (serialized_length > sizeof(context->tx_buffer)) ||
        (serialized_length > UINT16_MAX)) {

        return false;
    }

    context->tx_length = (uint16_t)serialized_length;

    return true;
}

static bool coapFirmwareDownloadSendPreparedRequest(coap_firmware_download_context_t *context) {
    if ((context == NULL) || (context->tx_length == 0U)) return false;

    if (!netDatagramSendTo(context->tx_buffer, context->tx_length, context->remote_host, context->parsed_uri.port))
        return false;

    context->status.tx_count++;

    context->response_deadline_ms = otaPlatformMillis() + context->retry_timeout_ms;

    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_WAIT_RESPONSE;

    return true;
}

// Response handling
static bool coapFirmwareDownloadTokenMatches(coap_firmware_download_context_t *context, coap_packet_t *response) {
    uint8_t *response_token = NULL;

    int response_token_length;

    response_token_length = coap_get_header_token(response, &response_token);

    if ((response_token_length != (int)COAP_FIRMWARE_TOKEN_LENGTH) || (response_token == NULL)) return false;

    return memcmp(response_token, context->token, COAP_FIRMWARE_TOKEN_LENGTH) == 0;
}

static bool coapFirmwareDownloadProcessResponse(coap_firmware_download_context_t *context, const uint8_t *data, uint16_t length) {
    coap_packet_t response;

    const uint8_t *payload = NULL;

    size_t payload_length;

    uint32_t block_number = 0U;
    uint32_t block_offset = 0U;
    uint32_t total_size = 0U;

    uint16_t block_size = 0U;

    uint8_t more = 0U;

    firmware_update_service_status_t service_status;

    if ((context == NULL) || (data == NULL) || (length == 0U)) return false;

    memset(&response, 0, sizeof(response));

    if (coap_parse_message(&response, (uint8_t *)data, length) != NO_ERROR) return false;

    if (!coapFirmwareDownloadTokenMatches(context, &response)) {
        coap_free_header(&response);

        return false;
    }

    context->status.rx_count++;
    context->status.last_response_mid = response.mid;
    context->status.last_response_code = response.code;
    context->status.last_response_length = length;

    if ((response.code != CONTENT_2_05) || (response.mid != context->request_mid)) {
        coap_free_header(&response);

        coapFirmwareDownloadStopWithError(context,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INVALID_URI,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

        return true;
    }

    payload_length = coap_get_payload(&response, &payload);

    if ((payload == NULL) || (payload_length == 0U)) {
        coap_free_header(&response);

        coapFirmwareDownloadStopWithError(context,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

        return true;
    }

    /*
     * --------------------------------------------------------------
     * Block2
     * --------------------------------------------------------------
     */
    if (coap_get_header_block2(&response, &block_number, &more, &block_size, &block_offset)) {
        if ((block_number != context->block_number) || (block_size == 0U) || (context->service == NULL) || (block_offset != context->service->download_offset)) {
            coap_free_header(&response);

            coapFirmwareDownloadStopWithError(context,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

            return true;
        }
    }
    else {
        if (context->block_number != 0U) {
            coap_free_header(&response);

            coapFirmwareDownloadStopWithError(context,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

            return true;
        }

        block_size = (uint16_t)payload_length;

        block_offset = 0U;
        more = 0U;
    }

    /*
     * --------------------------------------------------------------
     * 첫 Block → 전체 Firmware 크기 확보
     * --------------------------------------------------------------
     */
    if (context->total_size == 0U) {
        if ((!coap_get_header_size(&response, &total_size)) || (total_size == 0U)) {
            coap_free_header(&response);

            coapFirmwareDownloadStopWithError(context,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

            return true;
        }

        context->total_size = total_size;

        context->status.total_size = total_size;

        service_status = firmware_update_service_begin_download(context->service, context->total_size);

        if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK) {
            coap_free_header(&response);

            coapFirmwareDownloadStopForServiceFailure(context);

            return true;
        }
    }

    /*
     * Payload가 전체 Image 범위를 넘는지 검사.
     */
    if ((block_offset > context->total_size) || (payload_length > context->total_size - block_offset)) {
        coap_free_header(&response);

        coapFirmwareDownloadStopWithError(context,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

        return true;
    }

    /*
     * --------------------------------------------------------------
     * 실제 Firmware Chunk 전달
     * --------------------------------------------------------------
     */
    service_status = firmware_update_service_write_chunk(context->service, payload, payload_length);

    coap_free_header(&response);

    if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK) {
        coapFirmwareDownloadStopForServiceFailure(context);

        return true;
    }

    context->status.downloaded_size = (uint32_t)context->service->download_offset;
    context->status.block_number = block_number;
    context->status.block_size = block_size;
    context->status.retry_count = 0U;

    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_FIRMWARE_INITIAL_RETRY_TIMEOUT_MS;

    /*
     * --------------------------------------------------------------
     * 마지막 Block
     * --------------------------------------------------------------
     */
    if (more == 0U) {
        if (context->service->download_offset != context->total_size) {
            coapFirmwareDownloadStopWithError(context,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

            return true;
        }

        service_status = firmware_update_service_finish_download(context->service);

        if (service_status != FIRMWARE_UPDATE_SERVICE_STATUS_OK) {
            coapFirmwareDownloadStopForServiceFailure(context);

            return true;
        }

        context->status.completed_count++;
        context->status.active = false;
        context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_IDLE;
        context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_NONE;

        context->service = NULL;

        return true;
    }

    /*
     * --------------------------------------------------------------
     * 다음 Block 번호 계산
     * --------------------------------------------------------------
     */
    if ((context->service->download_offset % block_size) != 0U) {

        coapFirmwareDownloadStopWithError(context,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_RESPONSE);

        return true;
    }

    context->block_size = block_size;
    context->block_number = (uint32_t)(context->service->download_offset / block_size);

    context->status.block_number = context->block_number;
    context->status.block_size = context->block_size;

    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_REQUEST_PENDING;

    return true;
}

// Datagram Interceptor
static bool coapFirmwareDownloadHandleDatagram(const uint8_t *data, uint16_t length, const char *remote_host, uint16_t remote_port, void *raw_context) {
    coap_firmware_download_context_t *context;

    coap_packet_t response;

    bool token_match;

    (void)remote_host;

    context = (coap_firmware_download_context_t *)raw_context;

    if ((context == NULL) || (context != &s_download) || (!context->status.active) || (data == NULL) || (length == 0U))
        return false;

    if (remote_port != context->parsed_uri.port) return false;

    if (context->status.state != COAP_FIRMWARE_DOWNLOAD_STATE_WAIT_RESPONSE) {
        memset(&response, 0, sizeof(response));

        if (coap_parse_message(&response, (uint8_t *)data, length) != NO_ERROR) return false;

        token_match = coapFirmwareDownloadTokenMatches(context, &response);

        coap_free_header(&response);

        return token_match;
    }

    return coapFirmwareDownloadProcessResponse(context, data, length);
}

static bool coapFirmwareDownloadEnsureInterceptor(void) {
    if (s_download.status.interceptor_registered) return true;

    if (!wakaamaNetworkSetDatagramInterceptor(coapFirmwareDownloadHandleDatagram, &s_download)) return false;

    s_download.status.interceptor_registered = true;

    return true;
}

// URI / start
static firmware_download_transport_status_t
coapFirmwareDownloadStart(void *raw_context, const char *uri, size_t uri_length, firmware_update_service_t *service) {
    coap_firmware_download_context_t *context;

    coap_firmware_uri_status_t uri_status;

    context = (coap_firmware_download_context_t *)raw_context;

    if ((context == NULL) || (context != &s_download) || (service == NULL) || (!context->status.initialized))
        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    if (context->status.active) {
        context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_BUSY;

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;
    }

    coapFirmwareDownloadClearSession(context);

    if ((uri == NULL) || (uri_length == 0U)) {
        context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_URI;

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
    }

    if (uri_length > COAP_FIRMWARE_DOWNLOAD_URI_MAX_LENGTH) {
        context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_URI_TOO_LONG;

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
    }

    memcpy(context->request_uri, uri, uri_length);

    context->request_uri[uri_length] = '\0';

    uri_status = coapFirmwareUriParse(context->request_uri, uri_length, &context->parsed_uri);

    if (uri_status != COAP_FIRMWARE_URI_STATUS_OK) {
        if (uri_status == COAP_FIRMWARE_URI_STATUS_UNSUPPORTED_SCHEME)
            context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_UNSUPPORTED_PROTOCOL;
        else 
            context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_URI;

        return coapFirmwareDownloadMapUriStatus(uri_status);
    }

    if (!coapFirmwareDownloadCopySlice(context->remote_host,
                                       sizeof(context->remote_host),
                                       context->parsed_uri.host,
                                       context->parsed_uri.host_length) ||
        (context->remote_host[0] == '\0') ||

        !coapFirmwareDownloadCopySlice(context->path,
                                       sizeof(context->path),
                                       context->parsed_uri.path,
                                       context->parsed_uri.path_length) ||

        !coapFirmwareDownloadCopySlice(context->query,
                                       sizeof(context->query),
                                       context->parsed_uri.query,
                                       context->parsed_uri.query_length)) {

        context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_INVALID_URI;

        return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INVALID_URI;
    }

    context->service = service;

    context->block_number = 0U;
    context->block_size = COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE;

    context->total_size = 0U;

    context->retry_count = 0U;
    context->retry_timeout_ms = COAP_FIRMWARE_INITIAL_RETRY_TIMEOUT_MS;

    context->last_progress_percent = 0U;

    context->status.uri_length = uri_length;
    context->status.remote_port = context->parsed_uri.port;
    context->status.block_number = 0U;
    context->status.block_size = COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE;
    context->status.total_size = 0U;
    context->status.downloaded_size = 0U;
    context->status.retry_count = 0U;
    context->status.request_count++;
    context->status.active = true;
    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_REQUEST_PENDING;
    context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_NONE;

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

// Cancel
static firmware_download_transport_status_t coapFirmwareDownloadCancel(void *raw_context) {
    coap_firmware_download_context_t *context;

    context = (coap_firmware_download_context_t *)raw_context;

    if ((context == NULL) || (context != &s_download)) return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_INTERNAL_FAILURE;

    context->status.cancel_count++;
    context->status.active = false;
    context->status.state = COAP_FIRMWARE_DOWNLOAD_STATE_IDLE;
    context->status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_NONE;

    /*
     * Service cancel은 object_firmware가 이어서 수행하므로
     * 여기서는 Transport Session만 종료합니다.
     */
    coapFirmwareDownloadClearSession(context);

    return FIRMWARE_DOWNLOAD_TRANSPORT_STATUS_OK;
}

// functions
bool coapFirmwareDownloadTransportInit(firmware_download_transport_t *transport) {

    if (transport == NULL) return false;

    memset(&s_download, 0, sizeof(s_download));

    memset(transport, 0, sizeof(*transport));

    s_download.public_transport = transport;

    transport->context = &s_download;
    transport->start = coapFirmwareDownloadStart;
    transport->cancel = coapFirmwareDownloadCancel;

    s_download.block_size = COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE;

    s_download.retry_timeout_ms = COAP_FIRMWARE_INITIAL_RETRY_TIMEOUT_MS;

    s_download.status.initialized = true;
    s_download.status.state = COAP_FIRMWARE_DOWNLOAD_STATE_IDLE;
    s_download.status.block_size = COAP_FIRMWARE_DOWNLOAD_BLOCK_SIZE;
    s_download.status.last_error = COAP_FIRMWARE_DOWNLOAD_ERROR_NONE;

    return true;
}

void coapFirmwareDownloadTransportProcess(void) {
    net_datagram_status_t network_status;
    uint32_t now;

    if (!s_download.status.initialized) return;

    if (!coapFirmwareDownloadEnsureInterceptor()) return;

    if (!s_download.status.active) return;

    network_status = netDatagramGetStatus();

    /*
     * --------------------------------------------------------------
     * 새 Block Request
     * --------------------------------------------------------------
     */
    if (s_download.status.state == COAP_FIRMWARE_DOWNLOAD_STATE_REQUEST_PENDING) {
        if (!netDatagramIsReady()) return;
        if (netDatagramIsTxBusy()) return;

        if (!coapFirmwareDownloadPrepareRequest(&s_download)) {
            coapFirmwareDownloadStopWithError(&s_download,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_INTERNAL,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_PACKET_BUILD);

            return;
        }

        if (!coapFirmwareDownloadSendPreparedRequest(&s_download)) {
            coapFirmwareDownloadStopWithError(&s_download,
                                              FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                                              COAP_FIRMWARE_DOWNLOAD_ERROR_SEND_FAILED);

            return;
        }

        return;
    }

    /*
     * --------------------------------------------------------------
     * Response 대기
     * --------------------------------------------------------------
     */
    if (s_download.status.state != COAP_FIRMWARE_DOWNLOAD_STATE_WAIT_RESPONSE) return;

    if (!netDatagramIsReady()) {
        coapFirmwareDownloadStopWithError(&s_download,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_NETWORK_NOT_READY);

        return;
    }

    /*
     * ESP-AT CIPSEND 자체 실패.
     */
    if (network_status.tx_state == NET_DATAGRAM_TX_FAILED) {
        coapFirmwareDownloadStopWithError(&s_download,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_SEND_FAILED);

        return;
    }

    if (netDatagramIsTxBusy()) return;

    now = otaPlatformMillis();

    if (!coapFirmwareDownloadDeadlineExpired(now, s_download.response_deadline_ms)) return;

    /*
     * Reference와 동일하게 최대 4회 Retry.
     */
    if (s_download.retry_count >= COAP_FIRMWARE_MAX_RETRY_COUNT) {
        coapFirmwareDownloadStopWithError(&s_download,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_RESPONSE_TIMEOUT);

        return;
    }

    s_download.retry_count++;

    s_download.status.retry_count = s_download.retry_count;

    /*
     * 2s → 4s → 8s → 8s
     */
    if (s_download.retry_timeout_ms < COAP_FIRMWARE_MAX_RETRY_TIMEOUT_MS) {
        s_download.retry_timeout_ms *= 2U;

        if (s_download.retry_timeout_ms > COAP_FIRMWARE_MAX_RETRY_TIMEOUT_MS)
            s_download.retry_timeout_ms = COAP_FIRMWARE_MAX_RETRY_TIMEOUT_MS;
    }
    
    if (!coapFirmwareDownloadSendPreparedRequest(&s_download)) {
        coapFirmwareDownloadStopWithError(&s_download,
                                          FIRMWARE_UPDATE_DOWNLOAD_FAILURE_CONNECTION_LOST,
                                          COAP_FIRMWARE_DOWNLOAD_ERROR_SEND_FAILED);

        return;
    }
}

bool coapFirmwareDownloadTransportIsActive(void) { 
    return s_download.status.initialized && s_download.status.active; 
}

coap_firmware_download_status_t coapFirmwareDownloadTransportGetStatus(void) { 
    return s_download.status; 
}