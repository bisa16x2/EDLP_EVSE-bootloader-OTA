# 시스템 구조

EDLP는 EVSE, BMS, 서버 및 GUI를 연결한 팀 프로젝트입니다. 이 저장소는 그중 EVSE의 Bootloader와 이식을 위한 OTA package를 다룹니다. 충전 제어 Application과 BMS·서버·GUI는 팀의 다른 담당 영역입니다.

```text
서버 → LwM2M / CoAP / UDP → ESP8266 → STM32 UART
                                      ↓
                        OTA_WIFI: download → Bank2 staging
                                      ↓
                  서버 Update 실행 → Install Pending
                                      ↓
                     Host Application: 안전 조건 확인 및 commit
                                      ↓
                        Backup SRAM 요청 → reset → Bootloader
                                      ↓
                     staging 검증 → Application 설치 → jump
```

OTA download는 실행 중인 Application과 분리된 Bank2에 기록합니다. 설치 시점은 host Application에서 결정합니다. Bootloader는 reset 이후 staging을 검증하고 Application 영역에 기록합니다. 설치 과정에서 기존 Application은 지워지므로 rollback을 보장하지 않습니다.

코드 진입점은 [Bootloader 흐름](02-bootloader-code-flow.md)과 [OTA 흐름](03-ota-code-flow.md)을 참고하세요.

## 저장소의 세 구성

| 구성 | 책임 | 탐색 시작점 |
| --- | --- | --- |
| `EVSE_prev_bootloader` | 별도 Flash 영역에서 실행하며 image 검증·설치 및 Application jump 수행 | [Bootloader README](../firmware/EVSE_prev_bootloader/README.md) |
| `bootloader-package` | Application에 이식하여 download·staging 및 설치 요청 처리 | [OTA package README](../firmware/bootloader-package/README.md) |
| `EVSE_test_application` | OTA package와 Bootloader 연동을 시험하는 Application | [Test Application README](../firmware/EVSE_test_application/README.md) |

Package는 Bootloader binary를 포함하지 않습니다. Test Application은 실제 충전 제어 Application이 아닙니다.
