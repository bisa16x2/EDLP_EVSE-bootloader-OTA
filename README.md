# EDLP EVSE Bootloader & OTA

STM32F429ZI 기반 EVSE의 firmware를 원격으로 내려받아 검증하고 설치하기 위해 개발한 Bootloader와 OTA/Wi-Fi add-on입니다. 5인 팀 프로젝트 EDLP에서 수행한 개인 작업을 코드 중심으로 정리했습니다.

해당 프로젝트의 전체 코드는 [팀 프로젝트 공개 GitHub](https://github.com/orgs/KCCI-Embedded-Device-Lifecycle-Platform/repositories)에서 확인할 수 있습니다.

## 개인 기여 핵심

- EVSE Bootloader 단독 설계·구현: Flash 영역 분리, UART update protocol, image 검증, staging 설치 및 Application jump
- ESP8266 Wi-Fi, UDP, Wakaama/LwM2M, CoAP Block2 download와 Bank2 staging 구성
- EVSE의 서버 연결과 서버에서 전달한 firmware update 명령 처리 구현
- Backup SRAM을 이용한 Bootloader 요청, 설치 결과와 다운로드 상태 복구
- OTA 기능을 `OTA_WIFI` package로 분리해 동료가 담당한 EVSE Application에 연결할 수 있도록 API와 platform adapter 구성
- 팀장으로서 인터페이스와 통합 흐름 조율

OTA 기능의 package화 과정에는 시간 제약으로 AI를 활용했습니다. Bootloader는 단독으로 설계·구현했습니다.

충전 제어 Application, BMS, 서버와 GUI 구현은 다른 팀 담당 영역입니다. 서버 자체는 동료가 담당했으며, 본인은 EVSE 측의 서버 연결과 명령 처리를 담당했습니다. 상세 구분은 [기여 범위](CONTRIBUTIONS.md), 직접 구현한 기능의 파일과 함수는 [개인 기여 코드 안내](docs/09-contribution-code-guide.md)를 참고하세요.

## Firmware 구성

| 구성 | 역할 | 현재 복사본의 build 상태 |
| --- | --- | --- |
| [Bootloader](firmware/EVSE_prev_bootloader/README.md) | Firmware 검증·설치 및 Application jump | CubeMX 생성 코드와 build 보조 설정 재구성 필요 |
| [OTA package](firmware/bootloader-package/README.md) | Application에서 download·staging 및 Bootloader 설치 요청 | Host Application의 CubeMX·HAL·RTOS 구성 필요 |
| [Test Application](firmware/EVSE_test_application/README.md) | OTA package와 Bootloader의 연동 시험 | CubeMX 생성 코드와 build 보조 설정 재구성 필요 |

`bootloader-package`는 Bootloader binary 자체가 아닌 Application용 `OTA_WIFI` add-on입니다. Test Application은 이 package의 연동을 시험하기 위한 예시이며 실제 충전 제어 Application은 동료 담당 영역입니다.

## OTA 동작 흐름

```text
LwM2M Server → ESP8266 / UDP → Firmware Object /5
→ CoAP Block2 download → Bank2 staging → image 검증
→ 서버 Update 실행 → Install Pending
→ host Application의 설치 승인 → Boot request / reset
→ Bootloader 검증 및 설치 → Application jump
```

Flash 영역과 Backup SRAM 배치는 [통신과 메모리](docs/04-protocol-and-memory.md)에서 확인할 수 있습니다.

OTA 경로에는 임베디드 환경의 경량화와 서버 호환성을 고려해 Wakaama 기반 LwM2M client와 CoAP를 사용했습니다. Firmware Object `/5`로 update를 관리하고 CoAP Block2로 image를 나누어 내려받습니다. 자세한 역할은 [설계 결정](docs/06-design-decisions.md)을 참고하세요.

## 저장소 구조

```text
.
├── firmware/
│   ├── EVSE_prev_bootloader/   # 독립 Bootloader 코드와 주요 설정
│   ├── bootloader-package/     # Application 이식용 OTA 및 Bootloader 연동 package
│   └── EVSE_test_application/  # package 연동 시험 Application
├── docs/                       # 구조, 코드 흐름, 통신 및 검증 설명
├── CONTRIBUTIONS.md            # 개인·팀 기여 범위
├── ATTRIBUTION.md              # 원본과 재구성 범위
└── CHANGELOG.md                # 포트폴리오 정리 이력
```

## 문서

- [시스템 구조](docs/01-system-architecture.md)
- [Bootloader 코드 흐름](docs/02-bootloader-code-flow.md)
- [OTA package 코드 흐름](docs/03-ota-code-flow.md)
- [통신과 메모리](docs/04-protocol-and-memory.md)
- [모듈 책임과 Application 연동](docs/05-module-responsibilities.md)
- [설계 결정](docs/06-design-decisions.md)
- [시험 및 검증](docs/07-test-and-debug.md)
- [문제 해결과 확인 사항](docs/08-troubleshooting.md)
- [개인 기여 코드 안내](docs/09-contribution-code-guide.md)

## Build 및 검증

Bootloader와 Test Application은 `.ioc`, 직접 작성 코드, linker script 및 주요 설정을 남긴 발췌본입니다. CubeMX 생성 코드, startup code, CMake preset과 toolchain 설정이 빠져 있어 현재 폴더만으로 build할 수 없습니다. 재생성 시 필요한 연결은 각 구성의 README를 참고하세요.

OTA package는 host의 STM32CubeMX 생성 코드, HAL 및 CMSIS-RTOS2 구성이 필요합니다. 시험용 binary는 포함하지 않습니다. Test Application을 재생성·build한 뒤 생성된 binary를 STM32CubeProgrammer로 Application Flash 주소 `0x08020000`에 기록할 수 있습니다.

[프로젝트 포트폴리오](https://app.notion.com/p/d27efaa78d7d83b98df481a72e2b371d)에는 OTA download, Bootloader 설치 및 실제 Application 통합에 대한 검증 기록이 있습니다. 작성자가 직접 설치하여 프로젝트 복원을 확인했습니다. 이 저장소 자체에는 생성 코드가 포함되지 않으며, 별도의 build 및 장비 시험 결과는 이번 정리 과정에서 확인하지 않았습니다. Notion 페이지는 접근 권한이 필요할 수 있습니다.

## 제한사항

- CRC32는 오류 검출에 사용하며 firmware 서명 검증을 제공하지 않습니다.
- 실행 Application을 설치 중 지우므로 rollback과 설치 중 전원 차단 복구를 보장하지 않습니다.
- Wi-Fi 설정은 예시 값으로 교체했습니다. 실행 환경에 맞게 별도로 설정해야 합니다.
- 외부 라이브러리의 license와 notice는 각 원본 파일을 참고하세요.

## 출처와 기여 범위

원본 repository와 재구성 내역은 [ATTRIBUTION.md](ATTRIBUTION.md), 개인·팀 기여 범위는 [CONTRIBUTIONS.md](CONTRIBUTIONS.md), 정리 내역은 [CHANGELOG.md](CHANGELOG.md)를 참고하세요.
