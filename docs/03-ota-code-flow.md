# OTA package 코드 흐름

기준 경로는 `firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/`입니다. [`EVSE_APP/AP/ap.c`](../firmware/bootloader-package/EVSE_APP/AP/ap.c)는 실제 충전 제어가 아닌 시험용 연결 예시입니다.

별도 [`EVSE_test_application`](../firmware/EVSE_test_application/README.md)은 이 package를 포함해 연동을 시험하는 Application입니다. Package가 Bootloader binary를 Application 내부에 포함하는 구조는 아닙니다.

1. [`TASK/WIFI/wifi_task.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/TASK/WIFI/wifi_task.c)의 `wifiTaskInit()`과 `wifiTaskProcess()`가 Wi-Fi 처리를 수행합니다. ESP8266의 AT command와 수신 parser는 [`DRIVER/esp8266.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/DRIVER/esp8266.c)에 있습니다.
2. [`TASK/OTA/ota_task.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/TASK/OTA/ota_task.c)의 `otaTaskProcess()`가 network와 OTA service를 진행합니다. [`MIDDLEWARE/OTA_SERVICE/ota_service.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA_SERVICE/ota_service.c)는 Wakaama client와 download transport를 연결합니다.
3. Firmware Object `/5`에서 받은 URI에 따라 [`coap_firmware_download_transport.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/TRANSPORT/coap_firmware_download_transport.c)가 CoAP Block2 응답을 처리하고 update service에 chunk를 전달합니다.
4. [`firmware_update_service.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/firmware_update_service.c)는 download와 update 상태를 관리합니다. [`stm32_firmware_update_backend.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/stm32_firmware_update_backend.c)는 staging 저장, vector 검사 및 metadata 복구를 담당합니다.
5. 서버가 Firmware Object `/5`의 Update를 실행하면 설치 요청이 설정됩니다. Host가 `otaTaskIsInstallPending()`을 확인하고 안전한 시점에 `otaTaskCommitInstall()`을 호출합니다. [`ota_boot_request.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/TASK/OTA/ota_boot_request.c)는 지연 후 Boot request와 reset을 처리합니다. 다운로드 완료 후 reset으로 `DOWNLOADED` 상태를 복구하면 설치 요청은 복구되지 않으므로 서버 Update 실행이 다시 필요합니다.

시험용 `ap.c`는 pending 상태에서 자동 commit합니다. 실제 EVSE에서는 충전 안전 조건을 확인하는 host 제어 흐름에 연결해야 합니다.

## Test Application과 이식

[`EVSE_test_application/EVSE_APP/AP/ap.c`](../firmware/EVSE_test_application/EVSE_APP/AP/ap.c)는 세 task와 UART callback을 연결하는 시험 예시입니다. 두 폴더의 `OTA_WIFI`는 별도 복사본이므로 이후 한쪽을 수정하면 동일성이 유지되는지 다시 확인해야 합니다. 각 폴더는 CubeMX 생성 코드 또는 host의 HAL·RTOS 구성이 필요합니다. 작성자가 직접 설치해 프로젝트 복원을 확인했으며, 별도의 build 및 장비 시험 결과는 이번 정리 과정에서 확인하지 않았습니다.
