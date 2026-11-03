# 모듈 책임과 Application 연동

| 영역 | 책임 | 코드 |
| --- | --- | --- |
| Bootloader App | 부팅 판단, 명령 및 update 흐름 | `firmware/EVSE_prev_bootloader/EVSE_BOOT/App/` |
| Bootloader BSP / HW | Flash 정책, Backup SRAM, UART 및 jump | `firmware/EVSE_prev_bootloader/EVSE_BOOT/BSP/`, `HW/` |
| Bootloader Middleware | frame parser와 CRC | `firmware/EVSE_prev_bootloader/EVSE_BOOT/Middleware/` |
| OTA DRIVER / BSP | ESP8266, Wi-Fi, UDP와 staging 접근 | `firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/`의 `DRIVER/`, `BSP/` |
| OTA Middleware | LwM2M, download, network bridge와 상태 관리 | `firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/MIDDLEWARE/` |
| OTA Platform / Task | STM32 adapter와 주기 처리 | `firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/PLATFORM/`, `TASK/` |

Host는 Wi-Fi와 OTA task를 초기화·주기 실행하고 UART HAL callback을 package handler로 전달합니다. 설치 시점은 `otaTaskIsInstallPending()`과 `otaTaskCommitInstall()`로 host가 결정합니다. 이식 조건과 예시는 [OTA package README](../firmware/bootloader-package/README.md)를 참고하세요.

`otaTaskIsInstallPending()`은 단순한 download 완료가 아니라 서버의 Update 실행으로 설정된 설치 요청을 나타냅니다. Test Application은 이 요청에 자동 commit하며, 실제 EVSE host는 안전 조건을 먼저 확인해야 합니다. Test Application 연결은 [해당 README](../firmware/EVSE_test_application/README.md)에 정리되어 있습니다.

`ThirdParty/wakaama`, CubeMX 생성 코드 및 HAL/CMSIS는 외부 또는 생성 코드입니다. 실제 충전 FSM과 안전 판단은 동료가 담당한 Application 영역입니다.
