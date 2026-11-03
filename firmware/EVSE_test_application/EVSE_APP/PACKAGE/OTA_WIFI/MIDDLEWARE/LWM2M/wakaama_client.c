#include "wakaama_client.h"
#include "wakaama_platform_stm32.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include "wakaama_network_port.h"
#include "OBJECTS/object_firmware.h"

#define WAKAAMA_CLIENT_OBJECT_COUNT             4U
#define WAKAAMA_CLIENT_SERVER_URI_MAX_LENGTH    160U

#define WAKAAMA_CLIENT_STEP_FAILURE_THRESHOLD   5U

#define WAKAAMA_CLIENT_ENDPOINT_NAME_MAX_LENGTH 64U

// tables
typedef struct {
    wakaama_client_status_t status;

    lwm2m_context_t *context;

    lwm2m_object_t *objects[WAKAAMA_CLIENT_OBJECT_COUNT];

    firmware_update_service_t *firmware_service;

    firmware_update_state_t notified_firmware_state;
    firmware_update_result_t notified_update_result;

    bool registration_refresh_required;

    uint8_t consecutive_step_failures;
    bool context_restart_requested;
} wakaama_client_context_t;

// inners
static wakaama_client_context_t s_client;

static wakaama_client_lwm2m_state_t wakaamaClientMapLwm2mState(lwm2m_client_state_t state) {
    switch (state) {
        case STATE_INITIAL:             return WAKAAMA_CLIENT_LWM2M_INITIAL;
        case STATE_BOOTSTRAP_REQUIRED:  return WAKAAMA_CLIENT_LWM2M_BOOTSTRAP_REQUIRED;
        case STATE_BOOTSTRAPPING:       return WAKAAMA_CLIENT_LWM2M_BOOTSTRAPPING;
        case STATE_REGISTER_REQUIRED:   return WAKAAMA_CLIENT_LWM2M_REGISTER_REQUIRED;
        case STATE_REGISTERING:         return WAKAAMA_CLIENT_LWM2M_REGISTERING;
        case STATE_READY:               return WAKAAMA_CLIENT_LWM2M_READY;
        default:                        return WAKAAMA_CLIENT_LWM2M_INITIAL;
    }
}

static void wakaamaClientSetError(wakaama_client_error_t error) { 
    s_client.status.last_error = error; 
}

static void wakaamaClientNotifyFirmwareChanges(void) {
    lwm2m_uri_t uri;

    if ((s_client.context == NULL) || (s_client.firmware_service == NULL)) return;

    if (s_client.context->state != STATE_READY) return;

    if (s_client.firmware_service->state != s_client.notified_firmware_state) {
        LWM2M_URI_RESET(&uri);

        uri.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;
        uri.instanceId = FIRMWARE_UPDATE_OBJECT_INSTANCE_ID;
        uri.resourceId = FIRMWARE_UPDATE_RESOURCE_STATE_ID;

        lwm2m_resource_value_changed(s_client.context, &uri);

        s_client.notified_firmware_state = s_client.firmware_service->state;
    }

    /*
     * --------------------------------------------------------------
     * /5/0/5 Update Result
     * --------------------------------------------------------------
     */
    if (s_client.firmware_service->update_result != s_client.notified_update_result) {
        LWM2M_URI_RESET(&uri);

        uri.objectId = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;
        uri.instanceId = FIRMWARE_UPDATE_OBJECT_INSTANCE_ID;
        uri.resourceId = FIRMWARE_UPDATE_RESOURCE_RESULT_ID;

        lwm2m_resource_value_changed(s_client.context, &uri);

        s_client.notified_update_result = s_client.firmware_service->update_result;
    }
}

static bool wakaamaClientBuildServerUri(const char *server_host, uint16_t server_port, char *buffer, size_t buffer_size) {
    int result;

    if ((server_host == NULL) || (buffer == NULL) || (buffer_size == 0U) || (server_port == 0U)) return false;

    result = snprintf(buffer, buffer_size, "coap://%s:%u", server_host, (unsigned int)server_port);

    if (result <= 0) return false;

    if ((size_t)result >= buffer_size) return false;

    return true;
}

static void wakaamaClientFreeObjects(void) {
    /*
     * /5 Firmware Update
     */
    if (s_client.objects[3] != NULL) {
        free_firmware_update_object(s_client.objects[3]);

        s_client.objects[3] = NULL;
    }

    /*
     * /3 Device
     */
    if (s_client.objects[2] != NULL) {
        free_object_device(s_client.objects[2]);

        s_client.objects[2] = NULL;
    }
    
    /*
     * /1 Server
     */
    if (s_client.objects[1] != NULL) {
        clean_server_object(s_client.objects[1]);

        lwm2m_free(s_client.objects[1]);

        s_client.objects[1] = NULL;
    }

    /*
     * /0 Security
     */
    if (s_client.objects[0] != NULL) {
        clean_security_object(s_client.objects[0]);

        lwm2m_free(s_client.objects[0]);

        s_client.objects[0] = NULL;
    }

    s_client.status.objects_created = false;
}

static void wakaamaClientCleanup(void) {
    if (s_client.context != NULL) {
        lwm2m_close(s_client.context);

        s_client.context = NULL;
    }

    wakaamaClientFreeObjects();

    s_client.firmware_service = NULL;
}

static bool wakaamaClientRestartContext(void) {
    char endpoint_name[WAKAAMA_CLIENT_ENDPOINT_NAME_MAX_LENGTH];

    size_t endpoint_length;

    int result;

    if ((!s_client.status.initialized) || (!s_client.status.started) || (!s_client.status.objects_created))
        return false;

    if (s_client.context != NULL) {
        if (s_client.context->endpointName == NULL) return false;

        endpoint_length = strlen(s_client.context->endpointName);

        if ((endpoint_length == 0U) || (endpoint_length >= WAKAAMA_CLIENT_ENDPOINT_NAME_MAX_LENGTH)) return false;

        memset(endpoint_name, 0, sizeof(endpoint_name));
        memcpy(endpoint_name, s_client.context->endpointName, endpoint_length);

        lwm2m_close(s_client.context);

        s_client.context = NULL;
    } 
    else {
        return false;
    }

    if (!wakaamaNetworkResetLwm2mRuntime()) return false;

    /*
     * --------------------------------------------------------------
     * New Wakaama Context
     * --------------------------------------------------------------
     */
    s_client.context = lwm2m_init(wakaamaNetworkGetUserData());

    if (s_client.context == NULL) {

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_INIT);

        return false;
    }

    if (!wakaamaPlatformRestoreNextMid(s_client.context)) {
        lwm2m_close(s_client.context);

        s_client.context = NULL;

        (void)wakaamaNetworkResetLwm2mRuntime();

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_MID_PERSISTENCE);

        return false;
    }

    result = lwm2m_configure(s_client.context, endpoint_name, NULL, NULL, WAKAAMA_CLIENT_OBJECT_COUNT, s_client.objects);

    if (result != COAP_NO_ERROR) {
        lwm2m_close(s_client.context);

        s_client.context = NULL;

        (void)wakaamaNetworkResetLwm2mRuntime();

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_CONFIG);

        return false;
    }

    if (!wakaamaNetworkAttachLwm2mContext(s_client.context)) {
        lwm2m_close(s_client.context);

        s_client.context = NULL;

        (void)wakaamaNetworkResetLwm2mRuntime();

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_ATTACH);

        return false;
    }

    /*
     * --------------------------------------------------------------
     * Client Runtime State Reset
     * --------------------------------------------------------------
     */
    s_client.status.context_configured = true;

    s_client.status.registered = false;

    s_client.status.network_open_requested = false;
    s_client.status.network_ready = wakaamaNetworkIsReady();

    s_client.registration_refresh_required = false;

    s_client.consecutive_step_failures = 0U;

    s_client.context_restart_requested = false;

    s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_RUNNING;
    s_client.status.lwm2m_state = wakaamaClientMapLwm2mState(s_client.context->state);
    s_client.status.last_step_result = COAP_NO_ERROR;

    wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_NONE);

    return true;
}

// functions
bool wakaamaClientInit(const wakaama_client_config_t *config) {
    char server_uri[WAKAAMA_CLIENT_SERVER_URI_MAX_LENGTH];

    int result;

    if (config == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_INVALID_ARGUMENT);

        return false;
    }

    if (s_client.status.initialized) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_ALREADY_INITIALIZED);

        return false;
    }

    if ((config->endpoint_name == NULL) || (config->endpoint_name[0] == '\0') || (config->server_host == NULL) ||
        (config->server_host[0] == '\0') || (config->server_port == 0U) || (config->local_port == 0U) ||
        (config->short_server_id == 0U) || (config->lifetime_seconds == 0U) ||
        (config->firmware_update_service == NULL) || (config->lifetime_seconds > (uint32_t)INT_MAX)) {

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_INVALID_ARGUMENT);

        return false;
    }

    memset(server_uri, 0, sizeof(server_uri));

    if (!wakaamaClientBuildServerUri(config->server_host, config->server_port, server_uri, sizeof(server_uri))) {

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_SERVER_URI);

        return false;
    }

    if (!wakaamaNetworkInit()) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_NETWORK_INIT);

        return false;
    }

    if (!wakaamaNetworkConfigure(0U, config->server_host, config->server_port, config->local_port)) {

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_NETWORK_CONFIG);

        return false;
    }

    s_client.status.network_configured = true;

    /*
     * ----------------------------------------------------------------------
     * /0 Security Object
     * ----------------------------------------------------------------------
     */
    s_client.objects[0] = get_security_object((int)config->short_server_id, server_uri, NULL, NULL, 0U, false);

    if (s_client.objects[0] == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_SECURITY_OBJECT);

        wakaamaClientCleanup();
        return false;
    }

    /*
     * ----------------------------------------------------------------------
     * /1 Server Object
     * ----------------------------------------------------------------------
     */
    s_client.objects[1] = get_server_object((int)config->short_server_id, "U", (int)config->lifetime_seconds, false);

    if (s_client.objects[1] == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_SERVER_OBJECT);

        wakaamaClientCleanup();
        return false;
    }

    /*
     * ----------------------------------------------------------------------
     * /3 Device Object
     * ----------------------------------------------------------------------
     */
    s_client.objects[2] = get_object_device();

    if (s_client.objects[2] == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_DEVICE_OBJECT);

        wakaamaClientCleanup();
        return false;
    }

    /*
     * ----------------------------------------------------------------------
     * /5 Firmware Update Object
     * ----------------------------------------------------------------------
     */
    s_client.objects[3] = get_firmware_update_object(config->firmware_update_service, config->firmware_download_transport);

    if (s_client.objects[3] == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_FIRMWARE_OBJECT);

        wakaamaClientCleanup();

        return false;
    }

    s_client.firmware_service = config->firmware_update_service;

    s_client.notified_firmware_state = FIRMWARE_UPDATE_STATE_IDLE;

    s_client.notified_update_result = FIRMWARE_UPDATE_RESULT_INITIAL;

    s_client.status.objects_created = true;

    s_client.context = lwm2m_init(wakaamaNetworkGetUserData());

    if (s_client.context == NULL) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_INIT);

        wakaamaClientCleanup();
        return false;
    }

    if (!wakaamaPlatformRestoreNextMid(s_client.context)) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_MID_PERSISTENCE);

        wakaamaClientCleanup();

        return false;
    }

    /*
     * Mandatory object:
     *
     * /0 Security
     * /1 Server
     * /3 Device
     */
    result = lwm2m_configure(s_client.context,
                             config->endpoint_name,
                             NULL,
                             NULL,
                             WAKAAMA_CLIENT_OBJECT_COUNT,
                             s_client.objects);

    if (result != COAP_NO_ERROR) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_CONFIG);

        wakaamaClientCleanup();
        return false;
    }

    s_client.status.context_configured = true;

    if (!wakaamaNetworkAttachLwm2mContext(s_client.context)) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_ATTACH);

        wakaamaClientCleanup();
        return false;
    }

    s_client.status.short_server_id = config->short_server_id;
    s_client.status.lifetime_seconds = config->lifetime_seconds;

    s_client.status.initialized = true;
    s_client.status.started = false;

    s_client.status.network_open_requested = false;
    s_client.status.network_ready = false;

    s_client.status.registered = false;

    s_client.consecutive_step_failures = 0U;
    s_client.registration_refresh_required = false;

    s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_STOPPED;

    s_client.status.lwm2m_state = wakaamaClientMapLwm2mState(s_client.context->state);

    s_client.status.last_step_result = COAP_NO_ERROR;

    wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_NONE);

    return true;
}

bool wakaamaClientStart(void) {
    if ((!s_client.status.initialized) || (s_client.context == NULL)) {
        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_CONTEXT_INIT);

        return false;
    }

    if (s_client.status.started) return true;

    s_client.status.started = true;

    s_client.status.network_open_requested = false;
    s_client.status.network_ready = false;

    s_client.status.registered = false;

    s_client.registration_refresh_required = false;

    s_client.consecutive_step_failures = 0U;
    s_client.context_restart_requested = false;

    s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_WAIT_NETWORK;

    s_client.status.lwm2m_state = wakaamaClientMapLwm2mState(s_client.context->state);

    s_client.status.last_step_result = COAP_NO_ERROR;

    wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_NONE);

    return true;
}

void wakaamaClientProcess(void) {
    time_t timeout_seconds;
    int result;

    bool previous_network_ready;

    if ((!s_client.status.initialized) || (!s_client.status.started)) return;

    if (s_client.context_restart_requested) {
        if (!wakaamaNetworkIsReady()) {
            s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_WAIT_NETWORK;
            return;
        }

        if (!wakaamaClientRestartContext()) {
            s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_ERROR;
            return;
        }

        return;
    }

    if (s_client.context == NULL) {
        s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_ERROR;
        return;
    }

    wakaamaNetworkProcess();

    previous_network_ready = s_client.status.network_ready;

    s_client.status.network_ready = wakaamaNetworkIsReady();

    if (previous_network_ready && (!s_client.status.network_ready)) {
        if (s_client.status.registered || (s_client.context->state == STATE_READY))
            s_client.registration_refresh_required = true;
    }

    /*
     * --------------------------------------------------------------
     * UDP Network 준비
     * --------------------------------------------------------------
     */
    if (!s_client.status.network_ready) {
        s_client.status.registered = false;

        if (wakaamaNetworkOpen()) {
            s_client.status.network_open_requested = true;
            s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_OPENING_NETWORK;
        } 
        else {
            s_client.status.network_open_requested = false;
            s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_WAIT_NETWORK;
        }

        return;
    }

    /*
     * --------------------------------------------------------------
     * UDP READY
     * --------------------------------------------------------------
     */
    s_client.status.network_open_requested = false;
    s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_RUNNING;

    /*
     * --------------------------------------------------------------
     * Network Reconnect Registration Refresh
     * --------------------------------------------------------------
     */
    if (s_client.registration_refresh_required) {
        if (s_client.context->state == STATE_READY) {
            result = lwm2m_update_registration(s_client.context, 0U, true);

            if (result != COAP_NO_ERROR) {
                s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_ERROR;
                s_client.status.registered = false;

                wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_STEP);
                return;
            }
        }

        s_client.registration_refresh_required = false;
        s_client.status.registered = false;
    }
    /*
     * Firmware Update Service 상태 변화:
     *
     * IDLE → DOWNLOADING
     * DOWNLOADING → DOWNLOADED
     * DOWNLOADED → UPDATING
     * ...
     *
     * 를 /5 Observe Notification으로 연결합니다.
     */
    wakaamaClientNotifyFirmwareChanges();

    timeout_seconds = 1;

    result = lwm2m_step(s_client.context, &timeout_seconds);

    s_client.status.last_step_result = result;

    s_client.status.lwm2m_state = wakaamaClientMapLwm2mState(s_client.context->state);

    if (result != COAP_NO_ERROR) {
        if (s_client.consecutive_step_failures < UINT8_MAX) s_client.consecutive_step_failures++;

        if (s_client.consecutive_step_failures >= WAKAAMA_CLIENT_STEP_FAILURE_THRESHOLD)
            s_client.context_restart_requested = true;

        s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_ERROR;
        s_client.status.registered = false;

        wakaamaClientSetError(WAKAAMA_CLIENT_ERROR_STEP);

        return;
    }

    s_client.consecutive_step_failures = 0U;

    if (s_client.context->state == STATE_READY) {
        s_client.status.registered = true;
        s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_READY;
    } 
    else {
        s_client.status.registered = false;
        s_client.status.runtime_state = WAKAAMA_CLIENT_RUNTIME_RUNNING;
    }
}

bool wakaamaClientIsReady(void) {
    return s_client.status.initialized && s_client.status.started && s_client.status.network_ready &&
           s_client.status.registered && (s_client.status.runtime_state == WAKAAMA_CLIENT_RUNTIME_READY);
}

void wakaamaClientDeinit(void) {
    wakaamaClientCleanup();

    memset(&s_client, 0, sizeof(s_client));
}

bool wakaamaClientIsInitialized(void) {
    return s_client.status.initialized;
}

wakaama_client_status_t wakaamaClientGetStatus(void) {
    return s_client.status;
}