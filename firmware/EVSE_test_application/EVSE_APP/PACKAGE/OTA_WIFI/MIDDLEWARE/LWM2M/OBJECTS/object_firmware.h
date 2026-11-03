#ifndef OTA_DEVICE_INTEGRATION_KIT_WAKAAMA_OBJECT_FIRMWARE_H
#define OTA_DEVICE_INTEGRATION_KIT_WAKAAMA_OBJECT_FIRMWARE_H

#include "liblwm2m.h"

#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_update_service.h"
#include "OTA_WIFI/MIDDLEWARE/OTA/firmware_download_transport.h"

/*
 * LwM2M Firmware Update Object
 *
 * /5/0/3 : State
 * /5/0/5 : Update Result
 */
#define FIRMWARE_UPDATE_OBJECT_INSTANCE_ID       0U

#define FIRMWARE_UPDATE_RESOURCE_STATE_ID        3U
#define FIRMWARE_UPDATE_RESOURCE_RESULT_ID       5U

#ifdef __cplusplus
extern "C" {
#endif

lwm2m_object_t *get_firmware_update_object(firmware_update_service_t *service, firmware_download_transport_t *download_transport);

void free_firmware_update_object(lwm2m_object_t *objectP);

#ifdef __cplusplus
}
#endif

#endif