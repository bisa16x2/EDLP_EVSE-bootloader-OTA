# STM32 EVSE OTA Test Application

## CubeMX 프로젝트 복원

1. STM32CubeMX에서 이 폴더의 `Application.ioc`를 열고 CMake 프로젝트를 생성합니다. 대상은 STM32F429ZI이며 firmware package 설정은 STM32Cube FW_F4 V1.28.3입니다.
2. 생성된 프로젝트 폴더의 루트에 이 폴더의 `EVSE_APP/`, `ThirdParty/`, `CMakeLists.txt`, `STM32F429xx_FLASH.ld`를 복사합니다. 시험 도구가 필요하면 `tools/`도 같은 위치에 복사합니다.
3. 생성된 프로젝트의 CMake·linker 설정과 복사한 파일의 차이를 확인하고, UART 및 FreeRTOS task 설정과 callback 연결을 확인한 뒤 build합니다. 기존 파일을 덮어쓰기 전에 생성된 파일과 차이를 확인하세요.

`.ioc`에는 `StartControlTask`, `StartWifiTask`, `StartOtaTask`가 weak task로 설정되어 있으며 실제 task 함수와 UART callback은 `EVSE_APP/AP/ap.c`에 있습니다. Linker script는 Application을 `0x08020000`에 배치합니다. Bootloader에서 jump할 때의 VTOR 설정과 reset을 통한 직접 부팅 시 vector table 설정은 재생성 후 별도로 확인해야 합니다.

Source의 Wi-Fi 및 server 값은 예시로 변경했습니다. 기존 연결 설정이 포함된 `TestApp.bin`과 CubeMX 생성 코드, CMake preset, startup code는 포함하지 않습니다. 작성자는 직접 설치해 프로젝트 복원을 확인했습니다. 별도의 build 및 실제 장비 시험 결과는 이번 정리 과정에서 확인하지 않았습니다.

STM32F429ZI 기반 EVSE OTA 기능을 실제 제어 Application에 통합하기 전에 독립적으로 구축하고 end-to-end 동작을 검증하기 위한 Test Application입니다.

ESP8266 Wi-Fi 연결부터 Wakaama/LwM2M registration, Firmware Object `/5`, CoAP Block2 download, Internal Flash Bank2 staging, image 검증, reset recovery 및 Bootloader handoff까지 전체 OTA pipeline을 한 프로젝트에서 실행합니다.

이 프로젝트에서 검증한 OTA/Wi-Fi 기능은 이후 `EVSE_APP/PACKAGE/OTA_WIFI`로 정리하여 별도의 add-on package로 추출했습니다. TestApp은 충전기 제어 로직이 없는 OTA reference/verification application이며 실제 EVSE Application 자체는 아닙니다.

External Flash는 사용하지 않습니다. W25Q128 staging 방식은 검토 후 폐기했으며 STM32F429ZI Internal Flash Bank2를 최종 staging 영역으로 사용합니다.

## 프로젝트 목적

- Custom Bootloader 이후 wireless OTA 전체 경로 구축
- ESP8266 AT command와 UDP transport 검증
- Wakaama 기반 LwM2M client와 server registration 검증
- Firmware Update Object `/5` 동작 검증
- CoAP Block2 firmware download와 retry 처리
- Internal Flash Bank2 staging erase/write/read-back 검증
- Firmware size, vector table 및 CRC32 검증
- Backup SRAM metadata와 reset recovery 검증
- Application에서 Bootloader로 전달하는 install handoff 검증
- OTA 기능을 실제 EVSE에 이식하기 전 package 경계와 public API 확립

## 전체 구조

```text
┌──────────────────────────────────────────┐
│ LwM2M Server                             │
│ Firmware Update Object /5                │
└────────────────────┬─────────────────────┘
                     │ UDP / CoAP
                     ▼
┌──────────────────────────────────────────┐
│ STM32 EVSE Test Application              │
│                                          │
│ ESP8266 Wi-Fi                            │
│   ↓                                      │
│ UDP Network Bridge                       │
│   ↓                                      │
│ Wakaama / LwM2M                          │
│   ↓                                      │
│ CoAP Block2 Downloader                   │
│   ↓                                      │
│ Firmware Update Service                  │
│   ↓                                      │
│ Internal Flash Bank2 Staging             │
│   ↓                                      │
│ Metadata Commit / DOWNLOADED             │
└────────────────────┬─────────────────────┘
                     │ Boot Request + Reset
                     ▼
┌──────────────────────────────────────────┐
│ Compatible Bootloader                    │
│ Staging 검증 → Application 설치 → 실행   │
└──────────────────────────────────────────┘
```

## Source 구조

```text
EVSE_APP/
├── AP/                         # TestApp 초기화 및 RTOS task entry
├── BSP/                        # TestApp LED 등 board 기능
├── COMMON/                     # Version, task 주기, integration 설정
├── HW/                         # TestApp hardware 접근
└── PACKAGE/OTA_WIFI/
    ├── BSP/                    # Wi-Fi, UDP, staging, boot metadata 정책
    ├── COMMON/                 # Memory map, Backup SRAM layout
    ├── DRIVER/                 # ESP8266 AT command/parser
    ├── MIDDLEWARE/
    │   ├── LWM2M/              # Wakaama client와 Object /5
    │   ├── NETWORK/            # Datagram abstraction와 task bridge
    │   ├── OTA/                # CoAP transport와 STM32 backend
    │   └── OTA_SERVICE/        # OTA facade와 runtime state
    ├── PLATFORM/STM32/         # UART/Flash/Backup SRAM/Time port
    └── TASK/
        ├── WIFI/               # Wi-Fi worker
        └── OTA/                # OTA runtime와 Boot request
```

## 대상 환경

| 항목 | 설정 |
|---|---|
| MCU/Board | STM32F429ZI / NUCLEO-F429ZI |
| Application version | `0.2.1` |
| RTOS | FreeRTOS / CMSIS-RTOS2 |
| Wi-Fi module | ESP8266 AT firmware |
| Debug UART | USART3 |
| ESP8266 UART | USART6 |
| Network | UDP |
| Device management | Eclipse Wakaama / LwM2M |
| Firmware download | CoAP Block2 |
| 기본 download block | 512 bytes |
| Staging storage | STM32 Internal Flash Bank2 |
| Image 검증 | Size, MSP, Reset Handler, CRC32 |

주요 의존성:

- STM32 HAL/CMSIS
- FreeRTOS와 CMSIS-RTOS2
- Eclipse Wakaama (`ThirdParty/wakaama`)
- ESP8266 AT firmware
- LwM2M/CoAP server
- Staging installer가 구현된 호환 Bootloader

Wakaama의 license와 notice는 `ThirdParty/wakaama`에 포함된 원문을 확인해야 합니다.

## 프로젝트 구성 파일

| 파일 | 역할 |
|---|---|
| `Application.ioc` | STM32CubeMX MCU peripheral, UART, GPIO, Clock 및 FreeRTOS 설정 원본 |
| `CMakeLists.txt` | Application source, `OTA_WIFI`, STM32CubeMX 및 Wakaama target 구성 |
| `STM32F429xx_FLASH.ld` | Application을 `0x08020000`부터 896 KiB 범위에 배치 |

이 파일들은 TestApp 검증 환경을 재현하는 기준입니다. 다른 board나 실제 EVSE 프로젝트로 이식할 때는 해당 프로젝트의 CubeMX, linker 및 build 설정에 동일한 memory/peripheral 조건을 반영해야 합니다.

## Flash memory map

```text
0x08000000 ┌──────────────────────────────┐
           │ Bootloader                  │ 128 KiB
0x08020000 ├──────────────────────────────┤
           │ Running Test Application    │ 896 KiB
0x08100000 ├──────────────────────────────┤
           │ OTA Staging                 │ 896 KiB
0x081E0000 ├──────────────────────────────┤
           │ Reserved                    │ 128 KiB
0x08200000 └──────────────────────────────┘
```

| 영역 | 주소 |
|---|---|
| Bootloader | `0x08000000` ~ `0x0801FFFF` |
| Running Application | `0x08020000` ~ `0x080FFFFF` |
| OTA Staging | `0x08100000` ~ `0x081DFFFF` |
| Reserved | `0x081E0000` ~ `0x081FFFFF` |

Application과 staging capacity는 각각 896 KiB입니다. `STM32F429xx_FLASH.ld`의 Application link 영역과 `ota_memory_config.h`의 memory map을 함께 유지해야 합니다.

External Flash address와 driver는 사용하지 않습니다.

## Backup SRAM layout

TestApp와 Bootloader가 공유하는 persistent contract입니다.

| Offset | 용도 |
|---:|---|
| `+0` | Boot request magic |
| `+4` | Boot request inverse magic |
| `+8` | Staging metadata magic |
| `+12` | Staging metadata inverse magic |
| `+16` | Staging image size |
| `+20` | Staging image CRC32 |
| `+24` | Bootloader update result |
| `+28` | Wakaama MID journal slot A |
| `+48` | Wakaama MID journal slot B |

Magic/inverse 또는 magic/commit 조합을 사용하여 기록 도중 reset된 값을 정상 metadata로 오인하지 않도록 했습니다.

Wakaama `nextMID`는 두 slot을 교대로 사용합니다. 새 slot commit 전까지 이전 정상 slot을 유지하여 reset 이후에도 Message ID sequence를 복구합니다.

## RTOS Task 구성

TestApp는 세 개의 main task 흐름으로 구성됩니다.

```text
Control Task
├─ Application/OTA 초기화
├─ Wi-Fi 상태 LED 반영
└─ Install Pending 확인 및 TestApp 자동 commit

WiFi Task
├─ ESP8266/Wi-Fi 상태머신
└─ UDP command/RX queue worker

OTA Task
├─ Datagram 처리
├─ Wakaama/LwM2M runtime
├─ CoAP firmware downloader
└─ Deferred Boot request/reset
```

WiFi Task와 OTA Task 사이의 network 접근은 CMSIS-RTOS2 message queue와 mutex로 분리합니다. ESP8266 UART와 UDP socket 제어는 WiFi Task가 담당하고 OTA Task는 datagram port interface만 사용합니다.

## Firmware update 흐름

```text
ESP8266 Wi-Fi 연결
→ UDP socket open
→ LwM2M server registration
→ Firmware Object /5 Package URI 수신
→ CoAP Block2 GET 반복
→ Bank2 staging erase/write/read-back
→ 전체 image 수신
→ Size/Vector/CRC32 검증
→ Staging metadata commit
→ DOWNLOADED
→ 서버의 Firmware Object /5 Update 실행
→ Install Pending
→ TestApp에서 Install Commit
→ Deferred Boot Request
→ Backup SRAM request commit
→ MCU Reset
→ Bootloader 설치
→ 새 Application 실행
```

CoAP downloader는 기본 512-byte block을 요청합니다. Server가 응답한 block number, size 및 offset을 검사하며 timeout 발생 시 2초부터 최대 8초까지 backoff하고 최대 4회 retry합니다.

### Firmware image 검증

1. 선언된 전체 크기와 수신 크기 일치
2. Image 크기가 Application/staging capacity 이내
3. Initial MSP의 8-byte alignment와 SRAM/CCMRAM 범위 확인
4. Reset Handler Thumb bit 확인
5. Reset Handler가 실제 Application image 범위에 존재하는지 확인
6. Staging Flash write 후 read-back 확인
7. 전체 image CRC32 계산 및 metadata 저장

CRC32는 전송 오류와 storage 손상을 검출하지만 firmware 제작자의 신원이나 악의적 변조를 인증하지는 않습니다.

## Reset recovery

TestApp 시작 시 Backup SRAM의 update result와 staging metadata를 읽습니다.

유효한 staging metadata가 있으면:

```text
Image size 검사
→ Staging vector 검사
→ Staging 전체 CRC32 재계산
→ Metadata CRC32 비교
→ DOWNLOADED 상태 복구 (Install Pending은 복구하지 않음)
```

Download 완료 후 reset되더라도 정상 staging image를 다시 받지 않습니다. 설치를 진행하려면 서버가 Update를 다시 실행해야 합니다. Download 도중 reset된 경우에는 metadata가 commit되지 않았으므로 완료 image로 복구하지 않습니다.

Bootloader가 기록한 update result는 다음 Application 부팅 후 LwM2M Firmware Object 상태에 반영할 수 있도록 persistence합니다.

## Wi-Fi와 LwM2M recovery

- ESP8266 reset과 AP 재접속 state machine
- `WIFI DISCONNECT` 감지 후 retry
- UDP socket reopen
- Network 복구 후 LwM2M registration refresh
- 반복적인 Wakaama runtime 오류 시 context restart
- Context 재생성 시 Wakaama `nextMID` 복구
- Security, Server, Device, Firmware `/5` Object 재구성

이 기능은 장시간 동작 중 Wi-Fi 단절이나 server 재연결 이후 OTA service가 다시 사용할 수 있는 상태로 복귀하도록 구성했습니다.

## Install Pending과 TestApp 동작

Package는 download와 검증을 마치면 `DOWNLOADED` 상태가 됩니다. 이후 서버가 Firmware Object `/5`의 Update를 실행하면 설치 요청이 설정되고, 다음 API로 Install Pending 확인과 host의 설치 승인을 처리합니다. Download 완료만으로 Install Pending이 설정되거나 reset이 실행되지는 않습니다.

```c
bool otaTaskIsInstallPending(void);
bool otaTaskCommitInstall(void);
```

현재 TestApp의 `apControlMain()`은 Install Pending을 확인하면 즉시 `otaTaskCommitInstall()`을 호출합니다. 충전 제어 기능이 없는 TestApp에서 OTA E2E를 빠르게 검증하기 위한 의도적인 동작입니다.

실제 EVSE Application에서는 이 부분을 그대로 사용하면 안 됩니다. 다음과 같은 안전 조건을 확인한 뒤 commit해야 합니다.

```text
Install Pending
→ 신규 충전 시작 차단
→ 진행 중 충전 종료
→ Charging current 0 확인
→ Relay/Contactor OFF 확인
→ CAN/BMS 안전 상태 확인
→ otaTaskCommitInstall()
```

`otaTaskCommitInstall()`은 configured reset delay가 지난 뒤 Backup SRAM에 Boot request를 기록하고 MCU를 reset합니다. 이 delay는 LwM2M/CoAP 응답이 server에 전달될 시간을 확보하기 위한 것입니다.

## UART callback

ESP8266 수신은 UART interrupt를 사용합니다. HAL callback에서 package handler를 호출해야 합니다.

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    otaPlatformUartHandleRxCplt(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    otaPlatformUartHandleError(huart);
}
```

실제 EVSE에서 다른 UART module도 callback을 사용한다면 하나의 HAL callback에서 각 consumer로 dispatch해야 합니다. Package가 callback을 독점하게 두면 기존 UART 기능과 충돌할 수 있습니다.

## 설정

TestApp 연결 설정은 다음 파일에 있습니다.

```text
EVSE_APP/COMMON/ota_integration_config.h
```

설정 항목:

- Wi-Fi SSID/password
- LwM2M endpoint prefix
- Server host/port
- Local UDP port
- Short Server ID
- Registration lifetime
- Boot reset delay

현재 복사본의 Wi-Fi SSID·password와 server host는 예시 값입니다. 실제 사용 시 비공개 설정, build-time 주입 또는 provisioning 방식을 사용하세요.

## Build

필요 도구:

- CMake 3.22 이상
- Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`)
- STM32CubeMX 또는 포함된 CubeMX 생성 source

다음은 생성된 프로젝트에 해당 preset이 구성된 경우의 예시입니다. 현재 발췌본에는 `CMakePresets.json`이 없습니다.

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

정상적으로 build되면 `build/<Preset>/`에 다음 파일이 생성됩니다.

```text
Application.elf
Application.hex
Application.bin
```

기존 `TestApp.bin`은 포함하지 않습니다. 재생성 및 build 후 생성된 binary의 link 주소와 vector를 확인하세요.

## STM32CubeProgrammer 다운로드

STM32CubeProgrammer에서 재생성 및 build한 Application binary를 선택하고 Application Flash 시작 주소 `0x08020000`에 다운로드합니다.

```text
Application.bin → 0x08020000
```

Bootloader가 필요한 경우 별도 Bootloader binary를 먼저 다음 주소에 기록합니다.

```text
Bootloader.bin → 0x08000000
```

루트 `.gitignore`는 `*.bin`을 제외하므로 생성한 binary는 Git에 자동으로 추가되지 않습니다.

TestApp binary를 `0x08000000`에 기록하거나, 일반 `0x08000000` 기준 Application binary를 `0x08020000`에 기록하면 정상 실행되지 않습니다.

## 테스트 도구

| 파일 | 용도 |
|---|---|
| `tools/udp_echo_server.py` | ESP8266 UDP 송수신 확인 |
| `tools/coap_firmware_probe_server.py` | 단일 CoAP 요청/응답 확인 |
| `tools/coap_artifact_server.py` | Firmware binary를 제공하는 최소 CoAP Block2 시험 server |

이 도구들은 개발용이며 인증, 접근 제어 또는 production server 기능을 제공하지 않습니다.

## Bootloader 호환 조건

Wireless OTA install을 완료하려면 Bootloader가 다음 기능을 구현해야 합니다.

- 동일한 Backup SRAM offset과 magic 값 사용
- Remote Boot request 감지 및 clear
- Staging metadata size/CRC32 확인
- Bank1 erase 전에 Bank2 staging image 검증
- Staging image를 Application 영역으로 복사
- 설치 결과를 Backup SRAM에 기록
- 새 Application vector 검증 및 jump

TestApp는 Bank2 download와 install request까지만 담당하며 staging-to-Application 복사는 수행하지 않습니다. 단순히 remote request 후 command mode로 진입하는 Bootloader만 사용하면 wireless installation이 완료되지 않습니다.

## 제한사항과 주의사항

- External Flash와 rollback slot을 사용하지 않습니다.
- Bank2 staging은 download 중 기존 Application을 보존하지만 install 중 전원 차단 복구는 Bootloader에 의존합니다.
- 무결성 검증은 CRC32이며 digital signature 검증이 없습니다.
- 기본 network는 UDP/CoAP이며 DTLS가 구성되어 있지 않습니다.
- Firmware version, hardware model, downgrade 차단 정책이 없습니다.
- Wakaama `nextMID`는 persistence하지만 별도 firmware downloader MID allocator는 동일 journal을 사용하지 않습니다.
- Message queue depth가 제한되어 있으므로 장시간 task 지연과 burst packet을 주의해야 합니다.
- 실제 credential과 server 주소를 공개 source에 commit하면 안 됩니다.
- Application, staging 및 reserved Flash sector를 다른 module이 중복 사용하면 안 됩니다.
- TestApp의 자동 Install Commit은 실제 충전 중 EVSE에 그대로 적용하면 안 됩니다.
- Build 산출물과 source의 일치 여부를 별도로 관리해야 합니다.

## 프로젝트 범위

포함 범위:

- ESP8266 Wi-Fi와 UDP 통신
- Wakaama/LwM2M registration
- Firmware Object `/5`
- CoAP Block2 firmware download
- Internal Flash Bank2 staging
- Image 검증과 reset recovery
- Bootloader handoff
- OTA/Wi-Fi package 구조 검증

별도 책임:

- 실제 EVSE charging FSM, Relay 및 CAN/BMS 제어
- Bootloader staging installer
- Firmware signing과 key management
- Production credential provisioning
- Backend server 운영과 access control
