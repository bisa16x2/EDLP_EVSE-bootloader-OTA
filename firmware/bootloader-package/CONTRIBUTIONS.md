# 기여 범위와 코드 안내

OTA 기능의 package화 과정에는 시간 제약으로 AI를 활용했습니다. Bootloader는 단독으로 설계·구현했으며, 실제 EVSE 충전 제어 Application은 팀 동료의 담당 영역입니다.

EVSE 측에서 서버 연결과 firmware update 명령 처리는 본인이 담당했습니다. 서버 자체는 다른 팀 동료가 담당했습니다.

이 저장소는 5인 팀 프로젝트 EDLP의 EVSE OTA 기능을 이식 가능한 package로 정리한 자료입니다. 개발 기간은 2026년 8월 4일부터 8월 26일까지입니다. 전체 프로젝트의 배경과 검증 기록은 [EDLP 포트폴리오](https://app.notion.com/p/d27efaa78d7d83b98df481a72e2b371d)를 참고하세요.

## 개인 담당

| 영역 | 담당 내용 | 코드 위치 |
| --- | --- | --- |
| Bootloader | 단독 설계·구현: 부팅 제어, UART update, Flash 기록과 image 검증, staging image 설치 | [EVSE_BOOT 원본](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT) |
| Wi-Fi 및 통신 | ESP8266 AT command, Wi-Fi 연결 상태 관리와 UDP 통신 | `EVSE_APP/PACKAGE/OTA_WIFI/DRIVER`, `BSP`, `MIDDLEWARE/NETWORK` |
| OTA client | Wakaama 연동, Firmware Object `/5`, CoAP Block2 download와 update 상태 관리 | `EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/LWM2M`, `MIDDLEWARE/OTA` |
| Staging 및 handoff | Bank2 저장, image 검증, Backup SRAM metadata와 Bootloader 요청 | `EVSE_APP/PACKAGE/OTA_WIFI/BSP`, `PLATFORM/STM32`, `TASK/OTA` |
| Application 연동 | OTA 기능을 add-on package로 분리하고 task·callback·설치 시점 제어 API 제공 | `EVSE_APP/PACKAGE/OTA_WIFI`, `EVSE_APP/AP/ap.c` |
| 팀장 및 통합 | 역할·인터페이스·통합 순서 조율과 OTA/Bootloader 통합 검증 | [프로젝트 기록](https://app.notion.com/p/d27efaa78d7d83b98df481a72e2b371d) |

`EVSE_APP/AP/ap.c`는 시험용 Application의 연결 예시입니다. 실제 충전 제어 Application 전체를 의미하지 않습니다. Package는 설치 가능 상태를 제공하고, 실제 충전 상태를 고려한 설치 시점은 host Application에서 결정합니다.

## 팀과 외부 의존성

- EVSE 충전 제어 Application은 팀 동료의 담당 영역입니다. OTA package와의 통합은 팀 협업 범위입니다.
- BMS, 서버 및 GUI는 팀 프로젝트의 다른 담당 영역으로, 이 저장소에서 개인 단독 구현으로 주장하지 않습니다.
- `ThirdParty/wakaama`는 외부 Eclipse Wakaama 라이브러리입니다. STM32 연동과 Firmware Object 등 package 내부의 관련 코드는 별도로 구성했습니다. 재배포 시 해당 라이브러리의 license와 notice를 확인해야 합니다.
- STM32CubeMX 생성 코드와 STM32 HAL/CMSIS는 직접 작성한 기능과 구분합니다.

## 출처와 검증 범위

- 팀 프로젝트: [KCCI Embedded Device Lifecycle Platform](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform)
- OTA package 원본: [OTA-Package-stm32](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/OTA-Package-stm32)
- Bootloader 원본: [EVSE_BOOT](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT)
- 시험용 Application: [EVSE_TestApp](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_TestApp)

프로젝트 포트폴리오에는 Application jump, UDP, LwM2M 등록, download, CRC32 오류 거부, Bootloader 설치 및 실제 Application 통합의 성공 기록이 있습니다. 이 checkout에서 해당 시험을 다시 실행한 결과는 아닙니다. 현재 package checkout은 CubeMX 생성 build support가 빠져 있어 단독 build가 완결되지 않습니다. 자세한 구성과 제한은 [README.md](README.md)를 참고하세요.
