# EVSE Bootloader

> 이 폴더는 직접 작성 코드와 주요 설정을 남긴 발췌본입니다. CubeMX 생성 코드, startup code, CMake preset 및 toolchain 설정이 없어 아래 build 명령은 이 폴더에서 그대로 실행할 수 없습니다. 전체 build 구성은 [원본 repository](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT)를 참고하세요.

> CubeMX 재생성 시 `Core/Src/main.c`에 `App/ap_bootloader.h`를 include하고 peripheral 초기화 후 `ApBootloader_Init()`과 `ApBootloader_Run()`을 호출하는 사용자 코드를 복원해야 합니다. 보존된 linker script도 적용해야 합니다. 작성자는 직접 설치해 프로젝트 복원을 확인했으며, 별도의 build 결과는 이번 정리 과정에서 확인하지 않았습니다.

STM32F429ZI 기반 EVSE를 위한 Custom Bootloader입니다. Bootloader와 Application의 Flash 영역을 분리하고, UART binary protocol을 통해 Application firmware를 수신하여 내부 Flash에 기록·검증한 뒤 실행합니다.

이 repository는 Embedded Device Lifecycle Platform의 Bootloader 영역을 담당합니다. UART 기반 firmware update, Application 부팅 제어, 불완전 image 실행 방지 및 Backup SRAM 요청에 따른 Bank2 staging image 설치를 포함합니다.

External Flash와 rollback slot은 사용하지 않습니다. UART update는 Application 영역에 직접 기록하며, 원격 update는 내부 Flash Bank2 staging image를 검증한 뒤 Application 영역에 설치합니다.

## CubeMX 프로젝트 복원

1. STM32CubeMX에서 이 폴더의 `Bootloader.ioc`를 열고 CMake 프로젝트를 생성합니다.
2. 생성된 프로젝트 폴더의 루트에 이 폴더의 `EVSE_BOOT/`, `CMakeLists.txt`, `STM32F429xx_FLASH.ld`를 복사합니다. 시험 도구가 필요하면 `tools/`도 같은 위치에 복사합니다.
3. 생성된 `Core/Src/main.c`의 `USER CODE BEGIN Includes` 영역에 `#include "App/ap_bootloader.h"`를 추가하고, peripheral 초기화 후 `USER CODE BEGIN 2` 영역에서 `ApBootloader_Init()`과 `ApBootloader_Run()`을 호출합니다.
4. CMake 및 linker 설정이 복사한 파일과 생성된 CubeMX 구성에 맞는지 확인한 뒤 build합니다. 기존 파일을 덮어쓰기 전 생성된 파일과 차이를 확인하세요.

작성자는 직접 설치해 프로젝트 복원을 확인했습니다. 이번 문서 정리에서 별도의 build 결과는 확인하지 않았습니다.

## 주요 기능

- Bootloader와 Application의 독립적인 firmware 구성
- USER 버튼을 이용한 강제 Bootloader 진입
- Backup SRAM의 magic/inverse magic을 이용한 Application 원격 진입 요청
- Bank2 staging metadata, image size, vector 및 CRC32 검증 후 설치
- Application vector table의 MSP와 Reset Handler 유효성 검사
- UART text console과 binary OTA protocol
- CRC-16/CCITT-FALSE 기반 packet 오류 검출
- Application 영역 erase, word program 및 read-back 검증
- CRC32 기반 전체 firmware image 무결성 검증
- 순차 offset 검사로 누락·중복·순서가 바뀐 DATA packet 거부
- OTA update state machine과 ACK/NACK 응답
- 전체 검증 완료 후 vector table을 마지막에 기록하는 안전한 commit
- 검증된 Application으로 VTOR/MSP를 전환한 뒤 jump

## 대상 환경

| 항목 | 설정 |
|---|---|
| MCU/Board | STM32F429ZI / NUCLEO-F429ZI |
| Bootloader 시작 주소 | `0x08000000` |
| Bootloader 크기 | 128 KiB |
| Application 시작 주소 | `0x08020000` |
| OTA transport | USART3, 115200-8-N-1 |
| 최대 protocol data | 512 bytes |
| Packet 검증 | CRC-16/CCITT-FALSE |
| Firmware 검증 | CRC32 |

Application은 반드시 `0x08020000`을 기준으로 link하고 vector table offset도 동일한 주소에 맞춰야 합니다.

## 구조

```text
EVSE_BOOT/
├── App/          # Bootloader 실행 흐름, 명령 처리, OTA 상태머신
├── BSP/          # 보드 정책, UART transport, Application storage
├── Driver/       # UART byte-stream driver
├── HW/           # STM32 HAL/CMSIS 접근, Flash/GPIO/UART/System/Backup SRAM
├── Middleware/   # Hardware 독립 protocol, CRC16, CRC32
└── Common/       # 공통 설정, 오류 및 version 정의
```

의존 방향은 다음을 원칙으로 합니다.

```text
Core/main.c
    ↓
App ─────────────→ Middleware
 ↓
BSP
 ↓
Driver / HW
 ↓
STM32 HAL / CMSIS
```

`Middleware`는 UART, Flash 및 HAL에 의존하지 않습니다. 실제 peripheral 선택과 Application 주소 정책은 `BSP`가 담당하고, HAL/CMSIS 직접 접근은 `HW`에 제한합니다.

## Flash memory map

```text
0x08000000 ┌──────────────────────────────┐
           │ Bootloader                  │
           │ 0x08000000 ~ 0x0801FFFF     │
0x08020000 ├──────────────────────────────┤
           │ Application                 │
           │ 0x08020000 ~ 0x080FFFFF     │
0x08100000 ├──────────────────────────────┤
           │ Bank2 staging               │
0x081E0000 ├──────────────────────────────┤
           │ Reserved                    │
0x08200000 └──────────────────────────────┘
```

현재 Bootloader linker script는 `FLASH ORIGIN = 0x08000000`, `LENGTH = 128K`로 설정되어 있습니다. Application linker script는 이 repository에 포함되지 않습니다.

## 부팅 흐름

```text
Reset
  ↓
USER 버튼 요청 확인
  ↓
Backup SRAM 원격 요청 확인 및 일회성 clear
  ↓
요청이 있으면 staging 검증 및 설치 시도
  ↓
Application vector table 검사
  ├─ 설치 성공 또는 유효한 기존 Application → Application jump
  └─ Bootloader 진입 또는 Application 무효 → command loop
```

원격 진입 요청에는 Backup SRAM의 첫 두 word를 사용합니다.

| Offset | 값 |
|---:|---|
| `+0` | `0x45565345` |
| `+4` | `~0x45565345` |

Application은 magic을 먼저 기록하고 inverse magic을 commit 값으로 마지막에 기록한 뒤 MCU를 reset해야 합니다. Bootloader는 두 값이 모두 일치할 때만 요청을 인정하고, 감지 즉시 두 값을 지웁니다.

## 통신 방식

Bootloader 진입 직후에는 text mode로 시작합니다.

| Text command | 기능 |
|---|---|
| `HELLO` | 연결 확인 |
| `VERSION` | Bootloader version 확인 |
| `PROTO` | Binary protocol mode 진입 |
| `RUN` | 유효한 Application 실행 |

Binary frame은 다음 형식입니다.

```text
┌──────┬──────┬─────────┬────────────┬──────────┬────────────┐
│ SOF1 │ SOF2 │ Command │ Length LE  │ Data     │ CRC16 LE   │
│ AA   │ 55   │ 1 byte  │ 2 bytes    │ 0..512 B │ 2 bytes    │
└──────┴──────┴─────────┴────────────┴──────────┴────────────┘
```

CRC16 계산 범위는 `Command + Length + Data`이며 SOF는 제외합니다.

| Command | 값 | 기능 |
|---|---:|---|
| `HELLO` | `0x01` | Binary 연결 확인 |
| `GET_VERSION` | `0x02` | Bootloader version 조회 |
| `START_UPDATE` | `0x10` | Image size와 CRC32 전달, Application erase |
| `DATA` | `0x11` | Offset과 firmware block 전달 |
| `END_UPDATE` | `0x12` | Size, CRC32 및 vector 최종 검증 |
| `ABORT` | `0x13` | Update transaction 취소 및 `IDLE` 복귀 |
| `RUN_APP` | `0x20` | 유효한 Application 실행 |
| `NACK` | `0x1F` | 오류 응답 |
| `ACK` | `0x79` | 성공 응답 |
| `VERSION_RESPONSE` | `0x82` | Version 응답 |

## OTA update 흐름

```text
IDLE
  ↓ START_UPDATE
ERASING
  ↓
RECEIVING
  ↓ END_UPDATE
VERIFYING
  ├─ 검증 성공 → COMPLETE
  └─ 검증 실패 → FAILED

FAILED / 진행 중 상태
  ↓ ABORT
IDLE
```

일반적인 전송 순서는 다음과 같습니다.

```text
PROTO
→ HELLO
→ GET_VERSION
→ START_UPDATE(image_size, crc32)
→ DATA(offset, firmware_block) 반복
→ END_UPDATE
→ RUN_APP
```

### 불완전 image 부팅 방지

Application의 첫 8 bytes인 Initial MSP와 Reset Handler는 DATA 수신 중 Flash에 바로 기록하지 않고 RAM에 보관합니다. 모든 DATA를 받은 뒤 다음 검증이 성공한 경우에만 vector table을 Flash에 마지막으로 기록합니다.

1. 수신 크기와 선언된 image size 일치
2. 전체 image CRC32 일치
3. Initial MSP가 SRAM 또는 CCMRAM 범위에 존재
4. Reset Handler의 Thumb bit와 실행 주소가 유효
5. vector commit 후 실제 Flash vector table 재검사

따라서 전송 도중 reset 또는 전원 차단이 발생해도 부분적으로 기록된 image가 유효한 Application으로 판단되지 않습니다. 다만 현재 방식은 Application 영역을 직접 erase하므로 기존 Application을 보존하거나 rollback하지는 않습니다.

## Build

필요 도구:

- CMake 3.22 이상
- Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`)

Debug build:

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Release build:

```bash
cmake --preset Release
cmake --build --preset Release
```

정상적으로 build되면 `build/<Preset>/` 아래에 다음 파일이 생성됩니다.

```text
Bootloader.elf
Bootloader.hex
Bootloader.bin
```

## STM32CubeProgrammer 다운로드

Bootloader binary는 다음 주소에 기록합니다.

```text
Bootloader.bin → 0x08000000
```

초기 Application을 별도로 기록할 경우에는 `0x08020000` 기준으로 link된 Application binary를 사용합니다.

```text
Application.bin → 0x08020000
```

Bootloader 영역에 Application binary를 기록하거나, Application 시작 주소를 기준으로 link하지 않은 binary를 OTA 대상으로 사용하면 안 됩니다.

## 테스트

`tools/protocol_hello_test.py`는 serial 연결과 binary `HELLO/ACK`를 확인하는 기본 시험 도구입니다.

필요 패키지:

```bash
python -m pip install pyserial
```

실행 예:

```bash
python tools/protocol_hello_test.py /dev/ttyACM0
```

Windows에서는 `/dev/ttyACM0` 대신 `COM6`과 같은 port 이름을 사용합니다.

### Destructive test 주의

현재 `boot_config.h`에는 Flash storage, CRC32 및 update state machine 시험 기능이 활성화되어 있습니다. Flash/update 시험은 Application sector를 erase할 수 있으므로 시험용 보드와 복구 가능한 Application binary를 준비한 상태에서만 실행해야 합니다.

단, `FLASH_TEST` command는 설정 파일의 `BOOT_ENABLE_STORAGE_TEST`가 아닌 `BOOT_ENABLE_STORAGE_RESET_TEST` 조건으로 컴파일됩니다. 현재 제공된 기본 설정에는 후자가 정의되어 있지 않아 해당 command가 포함되지 않습니다. 시험 전에 build 설정과 command 조건을 확인해야 합니다.

공개 release를 만들기 전에는 다음 설정을 검토하십시오.

```c
#define BOOT_ENABLE_STORAGE_TEST 1
#define BOOT_ENABLE_CRC32_TEST   1
#define BOOT_ENABLE_UPDATE_TEST  1
```

## 프로젝트 완료 범위

이 repository의 최종 구현 범위는 다음과 같습니다.

- UART를 통한 Application 직접 update
- Packet 및 전체 image 검증
- 불완전 image 실행 방지
- USER 버튼 및 Application 요청 기반 Bootloader 진입
- Backup SRAM 기반 원격 진입 요청
- Bank2 staging image 사전 검증 및 Application 영역 설치
- Application vector table 검증과 안전한 jump
- 계층별 hardware/protocol 의존성 분리

다음 기능은 이 repository의 완료 기준에 포함하지 않습니다.

- External Flash 기반 staging 및 rollback
- Firmware metadata/version policy
- Cryptographic signature 검증
- 서버 기반 Wi-Fi/LwM2M/CoAP download 경로
- 실제 EVSE charging FSM과의 safe handover

서버 통신, Wi-Fi/LwM2M/CoAP download 및 Bank2 staging 기록은 별도의 TestApp/OTA package 영역에서 다룹니다. Bootloader는 원격 요청을 확인한 후 staging image를 검증하고 설치를 시도합니다.

## 제한사항

- UART update나 staging 설치가 Application 영역을 지운 이후 실패하면 기존 Application이 유지되지 않습니다.
- External Flash는 사용하지 않으며 별도의 rollback slot을 제공하지 않습니다.
- CRC32는 전송 오류 검출 및 무결성 확인 수단이며 firmware authenticity를 보장하지 않습니다.
- Remote boot request는 설치 요청 신호이며 firmware 자체는 Bank2 staging에 별도로 저장되어야 합니다.
- 실제 hardware 동작은 NUCLEO-F429ZI의 pin/UART 설정 및 Application linker 설정과 함께 검증해야 합니다.
