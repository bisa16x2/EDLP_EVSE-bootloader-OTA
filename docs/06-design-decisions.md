# 설계 결정

## Wakaama, LwM2M과 CoAP

임베디드 환경의 경량화와 서버 호환성을 고려해 Wakaama 기반 LwM2M client와 CoAP/UDP를 선택했습니다. 이 선택은 설계 의도이며, 다른 구현과의 메모리 사용량이나 성능을 비교 측정한 결과는 아닙니다.

- **Wakaama:** `ThirdParty/wakaama`를 LwM2M client 라이브러리로 사용하고, [`wakaama_client.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/LWM2M/wakaama_client.c)에서 STM32 환경과 서버 등록 흐름을 연결합니다.
- **LwM2M:** [`object_firmware.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/LWM2M/OBJECTS/object_firmware.c)의 Firmware Object `/5`를 통해 firmware URI, update 요청과 상태를 처리합니다.
- **CoAP:** [`coap_firmware_download_transport.c`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/OTA/TRANSPORT/coap_firmware_download_transport.c)에서 UDP datagram으로 Block2 요청을 보내고 응답을 나누어 받아 staging 경로로 전달합니다.

기본 경로에는 DTLS가 구성되어 있지 않으며, 호환성은 연결하는 서버의 설정과 지원 기능에 따라 별도로 확인해야 합니다.

## Application과 staging 분리

실행 Application은 Bank1, download image는 Bank2에 배치했습니다. 다운로드 중 실행 영역을 덮어쓰지 않으며, 설치 시에는 Bootloader가 Application 영역을 지웁니다. 별도 rollback slot은 없습니다. 설정은 [Flash와 Backup SRAM](04-protocol-and-memory.md)을 참고하세요.

## Vector 마지막 기록

update 중 Application의 첫 8바이트를 RAM에 보관하고 크기·CRC32·vector 확인 후 Flash에 기록합니다. 이 설계는 검증 이전에 부분 image의 vector가 유효해지는 상황을 줄이기 위한 것입니다. 설치 중 전원 차단으로부터 기존 Application을 복원하는 기능은 아닙니다. 구현은 [`ap_boot_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_update.c)에 있습니다.

## 설치 시점과 OTA 기능 분리

`OTA_WIFI` package는 다운로드 완료 상태와 서버 Update 실행 후의 install pending 상태를 구분하고 host가 commit 시점을 정하도록 구성했습니다. UART callback과 STM32 접근을 adapter로 분리해 동료의 Application과 연결하기 쉽게 했습니다. 시험용 예시의 자동 commit은 실제 충전 중 안전 판단을 대체하지 않습니다. Package화 과정에는 시간 제약으로 AI를 활용했습니다.

## Reset 이후 상태 전달

Backup SRAM에 magic/inverse와 image metadata를 기록해 Bootloader와 Application이 요청 및 결과를 교환합니다. 정상 reset과 기록 중 중단을 고려한 구조이지만 전원 조건에 따른 보존 여부는 별도 장비 검증이 필요합니다.

다운로드 완료 후 reset에서 metadata 검증에 성공하면 `DOWNLOADED`를 복구합니다. 설치 요청은 복구하지 않으므로 설치를 진행하려면 서버의 Update 실행이 다시 필요합니다.
