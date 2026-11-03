# 기여 범위와 출처

## 개인 담당

- Bootloader 전체 설계와 구현은 단독으로 수행했습니다.
- OTA/Wi-Fi client 및 staging 기능을 개발하고 동료가 담당한 EVSE Application에 이식할 수 있도록 package화했습니다.
- EVSE에서 서버에 연결하고 서버가 전달한 firmware update 요청을 처리하는 client 측 흐름을 담당했습니다.
- 팀장으로서 각 파트의 인터페이스, 통합 순서와 검증을 조율했습니다.

OTA 기능을 Application에 이식하기 위한 package화 과정에는 시간 제약으로 AI를 활용했습니다. 이 표기는 Bootloader의 단독 설계·구현 및 팀원 담당 영역과 구분하기 위한 것입니다.

시험용 Application은 Bootloader와 OTA 기능 확인을 위한 연결 예시입니다. EVSE 충전 제어 Application 전체가 아닙니다. 충전 제어 Application, BMS, 서버 및 GUI는 다른 팀 담당 영역이며, 서버 자체의 구현은 동료가 담당했습니다. 본인은 EVSE 측 서버 연결과 명령 처리를 담당했고, 전체 시스템 통합은 팀 협업 결과입니다.

`EVSE_prev_bootloader`는 독립 Bootloader, `bootloader-package`는 Application에 이식하는 OTA 및 Bootloader 연동 기능, `EVSE_test_application`은 이 기능을 시험하는 Application입니다. Package는 Bootloader 자체를 포함하지 않습니다.

## 코드 출처

- Bootloader: [팀 EVSE_BOOT](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT)
- OTA package: [팀 OTA-Package-stm32](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/OTA-Package-stm32)
- 시험용 Application: [팀 EVSE_TestApp](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_TestApp)
- [팀 GitHub 조직](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform)

원본의 Git 이력은 포함하지 않았으며 위 저장소에서 확인할 수 있습니다. STM32CubeMX 생성 코드, STM32 HAL/CMSIS와 `ThirdParty/wakaama`는 직접 작성한 코드와 구분합니다. Wakaama의 license와 notice는 해당 디렉터리에 보존되어 있습니다.

OTA package의 세부 코드 안내는 [기여 범위 안내](firmware/bootloader-package/CONTRIBUTIONS.md)를 참고하세요.
