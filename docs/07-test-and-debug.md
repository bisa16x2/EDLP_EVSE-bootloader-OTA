# 시험 및 검증

## 기존 프로젝트 기록

[EDLP Notion 포트폴리오](https://app.notion.com/p/d27efaa78d7d83b98df481a72e2b371d)에는 Application jump, UDP 송수신, LwM2M 등록, firmware download, CRC32 오류 거부, Bootloader 설치 및 실제 Application 통합을 PASS로 정리한 기록이 있습니다. 이는 기존 프로젝트 기록이며 이 복사본을 이번에 시험한 결과는 아닙니다. Notion 접근 권한이 필요할 수 있습니다.

## 저장소의 시험 코드

| 파일 | 용도 |
| --- | --- |
| `firmware/EVSE_prev_bootloader/tools/protocol_hello_test.py` | UART binary HELLO/ACK 확인 |
| `firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_crc32_test.c` | CRC32 시험 코드 |
| `firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_storage_test.c` | Flash storage 시험 코드 |
| `firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_update_test.c` | update state machine 시험 코드 |
| `firmware/bootloader-package/tools/` | UDP echo와 CoAP download 시험 서버 |

Flash 및 update 시험은 Application 영역을 지울 수 있습니다. 시험 코드가 있다는 사실만으로 이 복사본의 시험 통과를 의미하지 않습니다.

## 이번 복사본의 상태

| 구성 | 이번 복사본의 상태 |
| --- | --- |
| Bootloader | CubeMX 생성 코드와 CMake 보조 설정을 제외한 발췌본 |
| OTA package | `cmake/stm32cubemx`가 없어 host 구성 또는 재생성이 필요한 발췌본 |
| Test Application | CubeMX 생성 코드와 시험 binary를 제외한 발췌본 |

작성자가 직접 설치해 프로젝트 복원을 확인했습니다. 이 확인의 대상 구성과 build·실제 장비 동작 범위는 별도로 기록되어 있지 않습니다. 이번 문서 정리 과정에서 build, 자동 시험 및 실제 장비 시험 결과를 추가로 확인하지 않았습니다. Test Application 재생성 후 build한 binary는 STM32CubeProgrammer로 `0x08020000`에 기록합니다. Bootloader test 설정과 command 조건의 불일치는 [확인 사항](08-troubleshooting.md)에 기록했습니다.
