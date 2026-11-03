# 출처와 재구성 범위

이 저장소는 EDLP 팀 프로젝트의 EVSE Bootloader와 OTA package를 코드 포트폴리오로 재구성한 자료입니다. 개인과 팀의 역할은 [CONTRIBUTIONS.md](CONTRIBUTIONS.md)를 참고하세요.

## 원본

| 구성 | 원본 | 복사 기준 commit |
| --- | --- | --- |
| Bootloader | [EVSE_BOOT](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT) | `e7fefde7bd3a4b61e9655f20a60ad17f068ca002` |
| OTA package | [OTA-Package-stm32](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/OTA-Package-stm32) | `a33d2730810621b0c5bc199b94cb90188f6ccb60` |
| Test Application | [EVSE_TestApp](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_TestApp) | `83d4b55` |

복사 기준은 로컬 checkout이며 원본 Git history는 포함하지 않았습니다. OTA package README와 기여 안내는 원본 checkout에서 작성 중이던 문서 변경도 포함합니다. 설정 파일의 Wi-Fi SSID·password와 server host는 예시 값으로 교체했습니다. Bootloader README는 현재 코드와 발췌본 구성에 맞춰 수정했습니다.

Bootloader 복사본은 직접 작성한 `EVSE_BOOT`, `tools`, `Bootloader.ioc`, linker script, `CMakeLists.txt` 및 README만 남긴 발췌본입니다. 원본의 CubeMX 생성 파일과 CMake 보조 구성은 포함하지 않습니다.

Test Application은 CubeMX 생성 파일과 시험용 binary를 제외한 발췌본입니다. `ThirdParty/wakaama`는 OTA/LwM2M 기능의 필수 외부 의존성이며, package 이식과 Test Application 복원에 각각 포함합니다.

STM32CubeMX 생성 코드, STM32 HAL/CMSIS 및 `ThirdParty/wakaama`는 직접 작성 코드와 구분합니다. 외부 코드의 license와 notice는 함께 보존했습니다.
