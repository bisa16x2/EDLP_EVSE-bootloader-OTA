# 개인 기여 코드 안내

개인 담당 범위와 실제 파일을 연결한 탐색 경로입니다. 생성 코드와 외부 라이브러리는 직접 구현 범위에서 구분합니다.

## 단독 수행: Bootloader

| 기능 | 코드와 함수 |
| --- | --- |
| 부팅 판단 | [`ap_bootloader.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_bootloader.c)의 `ApBootloader_Init()`, `ApBootloader_Run()` |
| UART frame | [`boot_protocol.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Middleware/boot_protocol.c)의 `BootProtocol_ProcessByte()`, `BootProtocol_Encode()` |
| 명령 처리 | [`ap_boot_command.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_command.c)의 `ApBootCommand_ProcessPacket()` |
| Flash update | [`ap_boot_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_update.c)의 `ApBootUpdate_Start()`, `ApBootUpdate_WriteData()`, `ApBootUpdate_End()` |
| Staging 설치 | [`ap_boot_remote_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_remote_update.c)의 `ApBootRemoteUpdate_Install()` |
| Flash 접근 | [`bsp_storage.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/BSP/bsp_storage.c)의 erase·write·read-back |
| Application jump | [`hw_system.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/HW/hw_system.c)의 vector 검사와 jump |

## OTA 개발과 package화

OTA 기능을 Application에 이식하기 위한 package화 과정에는 시간 제약으로 AI를 활용했습니다.

서버 연결과 서버에서 전달한 update 명령의 EVSE 측 처리를 담당했습니다. 서버 자체의 구현은 팀 동료의 영역입니다.

| 기능 | 코드와 함수 |
| --- | --- |
| Wi-Fi / ESP8266 | [`wifi_task.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/TASK/WIFI/wifi_task.c), [`esp8266.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/DRIVER/esp8266.c) |
| 서버 연결 및 명령 | [`wakaama_client.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/LWM2M/wakaama_client.c), [`object_firmware.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/LWM2M/OBJECTS/object_firmware.c) |
| CoAP download | [`coap_firmware_download_transport.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/TRANSPORT/coap_firmware_download_transport.c)의 `coapFirmwareDownloadTransportProcess()` |
| Update 상태 | [`firmware_update_service.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/firmware_update_service.c) |
| STM32 staging | [`stm32_firmware_update_backend.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/stm32_firmware_update_backend.c), [`bsp_ota_storage.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/BSP/bsp_ota_storage.c) |
| Host 연결 | [`ota_task.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/TASK/OTA/ota_task.c)의 pending/commit API와 [`ap.c`](../firmware/bootloader-package/EVSE_APP/AP/ap.c)의 시험 예시 |

실제 EVSE 충전 제어는 동료 담당이고 전체 시스템 연결은 팀 협업입니다. 자세한 구분은 [기여 범위](../CONTRIBUTIONS.md)를 참고하세요.

시험용 연결 코드는 [`EVSE_test_application/EVSE_APP/AP/ap.c`](../firmware/EVSE_test_application/EVSE_APP/AP/ap.c)에서도 확인할 수 있습니다. 이 Application은 package와 Bootloader 연동 시험을 위한 예시입니다.
