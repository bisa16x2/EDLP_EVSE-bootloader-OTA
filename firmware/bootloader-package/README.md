# STM32 EVSE OTA/Wi-Fi Package

EDLP 팀 프로젝트에서 EVSE firmware의 원격 업데이트를 위해 개발한 OTA/Wi-Fi 기능을 Application에 이식하기 쉽도록 분리한 add-on package입니다. 이 저장소는 OTA package와 시험용 integration example을 담고 있습니다.

## 개인 기여 핵심

- STM32F429ZI EVSE Bootloader를 단독으로 설계하고 구현했습니다. Bootloader 소스는 [별도 저장소](https://github.com/KCCI-Embedded-Device-Lifecycle-Platform/EVSE_BOOT)에 있습니다.
- ESP8266 Wi-Fi, UDP, Wakaama/LwM2M 및 CoAP firmware download 경로를 구성했습니다.
- EVSE 측 서버 연결과 서버가 전달한 firmware update 명령 처리를 담당했습니다.
- 내부 Flash Bank2 staging, image 검증, Backup SRAM 기반 Bootloader 요청과 상태 복구를 구현했습니다.
- OTA 기능을 `OTA_WIFI` add-on으로 분리하고 동료가 담당한 EVSE Application에 연결할 수 있도록 API와 platform adapter를 구성했습니다.
- 팀장으로서 각 파트의 인터페이스와 통합 흐름을 조율했습니다.

실제 EVSE 충전 제어 Application과 BMS, 서버 및 GUI는 팀 프로젝트의 다른 담당 영역입니다. 서버 자체는 동료가 담당했으며, 본인은 EVSE 측의 연결과 명령 처리를 담당했습니다. 개인·팀 기여와 코드 탐색 경로는 [CONTRIBUTIONS.md](CONTRIBUTIONS.md)를 참고하세요.

프로젝트의 전체 구조와 검증 결과는 [EDLP 포트폴리오](https://app.notion.com/p/d27efaa78d7d83b98df481a72e2b371d)에 정리되어 있습니다. Notion 접근 권한에 따라 열람이 제한될 수 있습니다.

STM32F429ZI 기반 EVSE Application에 서버 OTA 기능을 추가하기 위해 TestApp에서 검증한 Wi-Fi, LwM2M, CoAP download, firmware staging 및 recovery 기능을 `OTA_WIFI` package로 분리한 프로젝트입니다.

이 package는 ESP8266을 통해 LwM2M server에 연결하고 Firmware Update Object `/5` 요청에 따라 새 Application image를 STM32 내부 Flash Bank2에 내려받습니다. 다운로드가 끝나면 image를 검증하고, host Application이 안전한 시점을 결정하여 Bootloader로 제어권을 넘길 수 있는 public API를 제공합니다.

External Flash는 사용하지 않습니다. W25Q128 기반 staging 방식은 검토 후 폐기했으며, 최종 구현은 STM32F429ZI의 Internal Flash Bank2를 staging 영역으로 사용합니다.

## OTA 통신 기술 선택

임베디드 환경의 경량화와 서버 호환성을 고려해 Wakaama, LwM2M과 CoAP/UDP를 사용했습니다.

- `ThirdParty/wakaama`의 Wakaama를 STM32 LwM2M client로 연동해 서버 등록과 runtime을 처리합니다.
- LwM2M Firmware Object `/5`에서 firmware URI와 Update 요청, 상태를 다룹니다.
- CoAP Block2로 firmware를 나누어 다운로드하고 수신 chunk를 Bank2 staging에 전달합니다.

이는 설계 의도이며 크기·성능 비교 측정 결과는 아닙니다. 기본 연결에는 DTLS가 구성되어 있지 않습니다.

## Application에 package 이식

1. 이 폴더의 `EVSE_APP/PACKAGE/OTA_WIFI/`를 기존 Application의 코드 폴더에 복사합니다.
2. 필수 의존성인 `ThirdParty/wakaama`를 Application 프로젝트에 추가하고, 함께 제공된 license와 notice를 유지합니다.
3. Host의 build 설정에 package source, Wakaama target과 include path를 연결합니다. 이 폴더의 `CMakeLists.txt`는 설정을 참고하기 위한 자료이며 기존 host 파일을 그대로 덮어쓸 필요는 없습니다.
4. Host의 STM32 HAL·CMSIS-RTOS2, UART handle과 callback, Wi-Fi 및 OTA task를 package에 연결합니다. `EVSE_APP/AP/ap.c`는 연결 예시이며, 실제 host의 `BSP/`, `HW/`와 충전 제어 코드는 host 프로젝트에서 제공합니다.
5. Host linker의 Application/Bank2 staging 영역과 Bootloader가 공유하는 Backup SRAM 설정을 대조하고, host의 안전 조건 확인 후 `otaTaskCommitInstall()`을 호출하도록 연결합니다. 아래 이식 체크리스트도 참고하세요.

이 폴더 자체는 Bootloader binary를 포함하지 않습니다. 전체 설치 흐름에는 별도의 호환 Bootloader가 필요합니다.

## Package 목적

TestApp에 결합되어 있던 OTA 기능을 실제 EVSE Application에 이식할 수 있도록 다음 책임을 하나의 add-on package로 정리했습니다.

- ESP8266 AT command 기반 Wi-Fi 연결과 자동 재연결
- UDP datagram transport
- Eclipse Wakaama 기반 LwM2M client
- LwM2M Firmware Update Object `/5`
- CoAP Block2 firmware download
- Internal Flash Bank2 staging
- Firmware size, vector table 및 CRC32 검증
- Backup SRAM 기반 staging metadata와 update result persistence
- Reset 후 다운로드 완료 상태 recovery
- Wakaama CoAP Message ID persistence
- WiFi Task와 OTA Task 사이의 queue/mutex network bridge
- Host Application이 설치 시점을 결정하는 pending/commit API

이 package는 충전 제어, Relay, CAN/BMS 및 EVSE 상태머신을 직접 제어하지 않습니다. 장치가 reset 가능한 시점은 host Application이 판단합니다.

## 전체 동작 구조

```text
┌──────────────────────────────────────────┐
│ LwM2M Server                             │
│ Firmware Object /5                       │
└────────────────────┬─────────────────────┘
                     │ UDP / CoAP
                     ▼
┌──────────────────────────────────────────┐
│ EVSE Application                         │
│                                          │
│ OTA_WIFI Package                         │
│ ├─ ESP8266 Wi-Fi                         │
│ ├─ UDP Network Bridge                    │
│ ├─ Wakaama / LwM2M                       │
│ ├─ CoAP Block2 Downloader                │
│ ├─ Firmware Update Service               │
│ ├─ Internal Flash Bank2 Staging          │
│ └─ Backup SRAM Persistence               │
│                                          │
│ Host EVSE FSM                             │
│ └─ 안전 조건 확인 후 Install Commit       │
└────────────────────┬─────────────────────┘
                     │ Boot Request + Reset
                     ▼
┌──────────────────────────────────────────┐
│ Compatible Bootloader                    │
│ ├─ Staging Metadata/Image 재검증          │
│ ├─ Bank1 Application 설치                │
│ ├─ Update Result 기록                    │
│ └─ 새 Application 실행                   │
└──────────────────────────────────────────┘
```

## Package 구조

```text
EVSE_APP/PACKAGE/OTA_WIFI/
├── BSP/
│   ├── bsp_wifi.*              # Wi-Fi 연결 상태머신
│   ├── bsp_udp.*               # ESP8266 UDP 제어
│   ├── bsp_ota_storage.*       # Bank2 staging 접근 정책
│   └── bsp_boot.*              # Backup SRAM metadata/request/result
├── COMMON/
│   ├── ota_memory_config.h     # Flash/RAM memory map
│   ├── boot_request_config.h   # Backup SRAM layout
│   └── ring_buffer.*
├── DRIVER/
│   └── esp8266.*               # ESP8266 AT command/parser
├── MIDDLEWARE/
│   ├── LWM2M/                  # Wakaama client와 Firmware Object /5
│   ├── NETWORK/                # Datagram abstraction와 task bridge
│   ├── OTA/                    # Download transport와 update backend
│   └── OTA_SERVICE/            # Package facade와 runtime state
├── PLATFORM/STM32/
│   ├── ota_platform_uart.*
│   ├── ota_platform_flash.*
│   ├── ota_platform_backup.*
│   └── ota_platform_time.*
└── TASK/
    ├── WIFI/                   # Wi-Fi worker
    └── OTA/                    # OTA runtime와 Boot request
```

`PLATFORM/STM32`만 STM32 HAL과 board handle에 직접 연결하고, OTA service와 protocol 계층은 상위 hardware 세부사항을 알지 않도록 구성했습니다.

## 대상 환경과 의존성

| 항목 | 설정 |
|---|---|
| MCU/Board | STM32F429ZI / NUCLEO-F429ZI |
| RTOS API | CMSIS-RTOS2 |
| Wi-Fi module | ESP8266 AT firmware |
| Debug UART | USART3 |
| ESP8266 UART | USART6 |
| Network | UDP |
| Device management | Wakaama / LwM2M |
| Firmware download | CoAP Block2 |
| 기본 download block | 512 bytes |
| Staging storage | STM32 Internal Flash Bank2 |
| Image 검증 | Size, MSP, Reset Handler, CRC32 |

주요 외부 의존성:

- STM32 HAL/CMSIS
- CMSIS-RTOS2 및 host project의 RTOS 구현
- Eclipse Wakaama (`ThirdParty/wakaama`)
- CubeMX에서 생성한 UART, GPIO, Clock 초기화 코드

Wakaama의 license와 notice는 `ThirdParty/wakaama`에 포함된 원문을 확인해야 합니다.

## Flash memory map

```text
0x08000000 ┌──────────────────────────────┐
           │ Bootloader                  │ 128 KiB
0x08020000 ├──────────────────────────────┤
           │ Running Application         │ 896 KiB
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

Application은 `0x08020000` 기준으로 link되어야 하며 최대 image 크기는 896 KiB입니다. Staging capacity도 동일하게 896 KiB입니다.

External Flash address나 driver는 사용하지 않습니다.

## Backup SRAM layout

Application과 Bootloader가 공유하는 persistent contract입니다. 양쪽의 주소, magic 및 결과 값이 반드시 동일해야 합니다.

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

Magic/inverse 또는 magic/commit 조합을 사용하여 metadata 기록 중 reset이 발생한 경우 불완전한 값을 정상 상태로 오인하지 않도록 했습니다.

Wakaama MID는 두 개의 slot을 번갈아 commit합니다. 새 slot 기록이 완료되기 전까지 이전 slot을 유지하므로 reset 이후 마지막 정상 `nextMID`를 복구할 수 있습니다.

## Firmware update 흐름

```text
Wi-Fi 연결
→ UDP open
→ LwM2M server registration
→ Firmware Object /5 Package URI 수신
→ CoAP Block2 GET 반복
→ Bank2 staging erase/write/read-back
→ 전체 image 수신 완료
→ Size/Vector/CRC32 검증
→ Staging metadata commit
→ DOWNLOADED
→ 서버의 Firmware Object /5 Update 실행
→ Install Pending
→ Host Application 안전 조건 확인
→ otaTaskCommitInstall()
→ Deferred Boot Request
→ Backup SRAM request commit
→ MCU Reset
```

CoAP downloader는 기본 512-byte Block2를 요청하며 server가 응답한 block size와 offset을 검사합니다. 응답 timeout은 2초부터 시작해 최대 8초까지 증가하며 최대 4회 retry합니다.

### Image 검증

다운로드 완료 시 다음 조건을 확인합니다.

1. 수신 크기가 server가 제공한 전체 크기와 일치
2. Image가 Application 및 staging capacity 이내
3. Initial MSP가 SRAM 또는 CCMRAM 범위에 존재
4. Reset Handler의 Thumb bit가 설정됨
5. Reset Handler가 해당 Application image 범위에 존재
6. Staging 전체 image CRC32 계산 완료
7. Flash write 후 read-back 성공

CRC32와 image metadata는 전송 손상 검출용입니다. Firmware 제작자 인증이나 악의적 변조 방지를 제공하지는 않습니다.

## Reset recovery

Package 초기화 시 Backup SRAM의 update result와 staging metadata를 확인합니다.

유효한 staging metadata가 있으면 다음 순서로 상태를 복구합니다.

```text
Metadata size 검사
→ Staging vector 검사
→ Staging 전체 CRC32 재계산
→ Metadata CRC32 비교
→ DOWNLOADED 상태 복구 (Install Pending은 복구하지 않음)
```

따라서 download 완료 후 reset되더라도 정상 staging image를 다시 받을 필요가 없습니다. Install Pending은 복구되지 않으며 설치를 진행하려면 서버가 Update를 다시 실행해야 합니다. 다운로드 도중 reset된 경우에는 metadata commit이 완료되지 않았으므로 완료 image로 복구하지 않습니다.

Bootloader가 기록한 update result도 Application 시작 시 읽어 LwM2M Firmware Object 상태에 반영할 수 있도록 유지합니다.

## Public API와 Host 통합

### Wi-Fi Task

```c
bool wifiTaskInit(const wifi_task_config_t *config);
void wifiTaskProcess(void);
bool wifiTaskIsConnected(void);
bool wifiTaskIsNetworkReady(void);
```

### OTA Task

```c
bool otaTaskInit(const ota_task_config_t *config);
void otaTaskProcess(void);
bool otaTaskIsInstallPending(void);
bool otaTaskCommitInstall(void);
```

기본 통합 순서:

```c
if (!wifiTaskInit(&wifi_config)) {
    /* 초기화 오류 처리 */
}

if (!otaTaskInit(&ota_config)) {
    /* 초기화 오류 처리 */
}
```

각 task에서는 process 함수를 지속적으로 호출해야 합니다.

```c
void WifiTask(void *argument)
{
    (void)argument;

    while (1) {
        wifiTaskProcess();
        osDelay(WIFI_TASK_PERIOD_MS);
    }
}

void OtaTask(void *argument)
{
    (void)argument;

    while (1) {
        otaTaskProcess();
        osDelay(OTA_TASK_PERIOD_MS);
    }
}
```

### 실제 EVSE의 Install Commit

제공된 `EVSE_APP/AP/ap.c`는 TestApp integration example이므로 Install Pending을 확인하면 즉시 `otaTaskCommitInstall()`을 호출합니다.

실제 EVSE에서는 이 동작을 그대로 사용하면 안 됩니다. Host FSM이 다음과 같은 안전 조건을 확인한 뒤 commit해야 합니다.

```text
Install Pending
→ 신규 충전 시작 차단
→ 진행 중 충전 종료
→ Charging current 0 확인
→ Relay/Contactor OFF 확인
→ CAN/BMS 안전 상태 확인
→ otaTaskCommitInstall()
```

`otaTaskCommitInstall()`은 즉시 reset하지 않습니다. 설정된 delay 동안 LwM2M/CoAP 응답이 전송될 시간을 확보한 후 Backup SRAM에 Boot request를 commit하고 MCU를 reset합니다.

## UART callback 통합

Package UART 수신은 interrupt 방식입니다. Host Application의 HAL callback에서 다음 handler를 호출해야 합니다.

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    otaPlatformUartHandleRxCplt(huart);

    /* 다른 UART consumer가 있으면 여기에서 함께 dispatch */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    otaPlatformUartHandleError(huart);

    /* 다른 UART consumer가 있으면 여기에서 함께 dispatch */
}
```

Package가 HAL callback을 독점하지 않도록 host Application에서 dispatcher 형태로 연결해야 합니다. USART handle이 다르면 `ota_platform_stm32_config.h`를 수정하십시오.

## 설정

Integration example의 연결 설정은 다음 파일에 있습니다.

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

공개 repository나 실제 제품에서는 credential을 source에 직접 commit하지 마십시오. 환경별 비공개 설정, build-time 주입 또는 별도 provisioning 방식을 사용해야 합니다.

## 이식 체크리스트

1. `EVSE_APP/PACKAGE/OTA_WIFI`를 host project에 추가
2. `ThirdParty/wakaama`와 해당 license/notice 포함
3. Host board에 필요한 peripheral 설정 반영
4. `STM32F429xx_FLASH.ld`의 Application/Bank2 memory 분할을 host linker 설정에 반영
5. `CMakeLists.txt`를 기준으로 package source, Wakaama target 및 include path 연결
6. CMSIS-RTOS2와 STM32 HAL 연결
7. USART3/USART6 handle 또는 platform mapping 조정
8. ESP8266 UART interrupt callback dispatch 연결
9. Wi-Fi와 LwM2M server 설정을 제품 환경에 맞게 주입
10. Bank2 staging sector를 다른 module이 사용하지 않는지 확인
11. Backup SRAM layout을 Bootloader와 동일하게 유지
12. WiFi Task와 OTA Task의 process loop 생성
13. EVSE FSM의 안전 조건 뒤에 `otaTaskCommitInstall()` 연결
14. 호환 Bootloader의 staging install 및 update result 동작 확인

## 프로젝트 구성 파일

Root에 있는 다음 파일은 `OTA_WIFI` package를 STM32 Application에 연결하고 검증할 때 사용하는 프로젝트 구성 파일입니다.

| 파일 | 역할 |
|---|---|
| `CMakeLists.txt` | `EVSE_APP`의 package source와 Wakaama library를 Application target에 포함하고 compile option, include path 및 BIN/HEX 생성을 구성 |
| `STM32F429xx_FLASH.ld` | 실행 Application을 `0x08020000`부터 최대 896 KiB로 link하여 Bank2 staging 영역과 분리 |
| `Application.ioc` | STM32CubeMX에서 MCU peripheral, UART, GPIO, Clock 및 RTOS 설정을 재생성·확인하기 위한 원본 project 설정 |

이 파일들은 package 내부 source만 복사할 때 반드시 함께 복사해야 하는 고정 요소는 아니지만, STM32F429ZI integration reference와 검증 환경을 재현하는 기준으로 사용합니다. 다른 host board에 이식할 때는 해당 board의 CubeMX project, linker script 및 build system에 같은 memory/peripheral 조건을 반영해야 합니다.

## Build 상태

이 repository는 package source와 Application integration example, 그리고 해당 package를 사용하기 위한 STM32 project 구성 파일을 보존한 추출본입니다. `CMakeLists.txt`는 `OTA_WIFI`, `ThirdParty/wakaama`, CubeMX 생성 source를 하나의 Application firmware로 build하도록 작성되어 있습니다.

현재 checkout의 `CMakeLists.txt`는 `cmake/stm32cubemx`를 참조하지만 해당 생성 디렉터리는 포함되어 있지 않습니다. Package를 기존 host Application의 CubeMX/HAL/RTOS 구성에 통합해야 합니다.

따라서 세 구성 파일은 실제 package integration에 사용하는 자료이지만, 현재 checkout만으로 standalone firmware build가 완결되는 것은 아닙니다.

일반적으로 필요한 include root는 다음과 같습니다.

```text
EVSE_APP/PACKAGE
ThirdParty/wakaama/include
ThirdParty/wakaama/examples/client/common
ThirdParty/wakaama/coap/er-coap-13
```

실제 source 목록과 Wakaama build option은 `CMakeLists.txt`를 참고하십시오.

## 테스트 도구

`tools/`에는 개발 및 통합 확인을 위한 보조 도구가 있습니다.

| 파일 | 용도 |
|---|---|
| `udp_echo_server.py` | ESP8266 UDP 송수신 확인 |
| `coap_firmware_probe_server.py` | 단일 CoAP 요청/응답 확인 |
| `coap_artifact_server.py` | Firmware binary를 제공하는 최소 CoAP Block2 시험 server |

이 도구들은 개발용이며 인증, 접근 제어 및 production 운영 기능을 제공하지 않습니다.

## Bootloader 호환 조건

이 package의 전체 install 경로를 사용하려면 Bootloader가 다음 기능을 구현해야 합니다.

- 동일한 Backup SRAM offset과 magic 값 사용
- Remote Boot request 감지 및 일회성 clear
- Staging metadata의 size/CRC32 확인
- Bank1 Application을 지우기 전에 Bank2 staging image 검증
- Staging image를 Application 영역으로 복사
- 설치 결과를 `BOOT_UPDATE_RESULT_OFFSET`에 기록
- 성공한 Application vector 검증 및 jump

현재 package는 Bootloader image 복사를 직접 수행하지 않습니다. 단순히 remote request를 감지해 command mode로 들어가는 Bootloader만 연결하면 wireless install은 완료되지 않습니다.

## 제한사항과 주의사항

- External Flash와 rollback slot을 사용하지 않습니다.
- Bank2 staging은 기존 실행 Application을 다운로드 중에 보존하지만, 설치 중 전원 차단 복구는 Bootloader 구현에 의존합니다.
- 현재 무결성 검증은 CRC32이며 digital signature 검증이 없습니다.
- 기본 network 경로는 UDP/CoAP이며 DTLS 기반 전송 보안이 구성되어 있지 않습니다.
- 동일 version 재설치와 downgrade를 차단하는 version policy가 없습니다.
- Wakaama `nextMID`는 persistence되지만 별도 firmware downloader의 MID allocator는 동일한 journal을 사용하지 않습니다.
- Queue depth가 제한되어 있으므로 task 주기 지연이나 burst packet에서 queue overflow 가능성을 검증해야 합니다.
- Wi-Fi credential과 server 주소를 공개 source에 하드코딩하지 않아야 합니다.
- Application, staging, reserved 영역의 sector를 다른 storage module과 중복 사용하면 안 됩니다.
- 실제 EVSE에서는 Install Pending 즉시 reset하지 말고 충전 안전 상태를 반드시 확인해야 합니다.

## 프로젝트 범위

이 repository의 완료 범위는 OTA/Wi-Fi 기능의 package화와 host integration interface 제공입니다.

포함 범위:

- Wi-Fi/LwM2M/CoAP 기반 firmware download
- Internal Flash Bank2 staging
- Image 검증과 persistent recovery
- Install Pending/Commit API
- STM32 platform abstraction
- TestApp integration example

별도 책임:

- 실제 EVSE charging FSM, Relay 및 BMS 제어
- Bootloader의 staging-to-Application installer
- Firmware signing 및 key management
- Production credential provisioning
- Backend server 운영과 access control
