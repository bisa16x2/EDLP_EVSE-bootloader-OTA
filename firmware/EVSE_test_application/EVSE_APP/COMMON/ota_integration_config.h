#ifndef EVSE_APP_COMMON_OTA_INTEGRATION_CONFIG_H
#define EVSE_APP_COMMON_OTA_INTEGRATION_CONFIG_H

/* -------------------------------------------------------------------------- */
/* Wi-Fi                                                                      */
/* -------------------------------------------------------------------------- */

#define OTA_INTEGRATION_WIFI_SSID        "Wi-Fi"
#define OTA_INTEGRATION_WIFI_PASSWORD    "password"

/* -------------------------------------------------------------------------- */
/* LwM2M Endpoint                                                             */
/* -------------------------------------------------------------------------- */

/*
 * 최종 Endpoint:
 *
 * <PREFIX>-<DEVICE_ID>
 *
 * Example:
 * EVSE-F429ZI-1234ABCD
 */
#define OTA_INTEGRATION_ENDPOINT_PREFIX  "EVSE-F429ZI"

/* -------------------------------------------------------------------------- */
/* LwM2M Server                                                               */
/* -------------------------------------------------------------------------- */

#define OTA_INTEGRATION_SERVER_HOST      "192.0.2.1"

#define OTA_INTEGRATION_SERVER_PORT      5683U
#define OTA_INTEGRATION_LOCAL_PORT       56830U

#define OTA_INTEGRATION_SHORT_SERVER_ID  123U
#define OTA_INTEGRATION_LIFETIME_SECONDS 300U

#define OTA_INTEGRATION_BOOT_RESET_DELAY_MS 1000U

#endif /* EVSE_APP_COMMON_OTA_INTEGRATION_CONFIG_H */