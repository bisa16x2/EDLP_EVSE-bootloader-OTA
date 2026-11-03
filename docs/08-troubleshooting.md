# 문제 해결과 확인 사항

## Flash 영역 중첩

프로젝트 기록에는 Application linker 영역이 Bank2 staging과 겹칠 수 있었던 문제가 정리되어 있습니다. Application 상한을 `0x08100000` 미만으로 제한하고 staging을 Bank2에 배치했습니다. 현재 주소는 [메모리 배치](04-protocol-and-memory.md)와 각 linker/config 파일을 함께 확인합니다.

## Application 이식 의존성

시험용 Application의 OTA 기능을 실제 EVSE에 연결할 때 UART callback, pin과 include 경로가 충돌할 수 있습니다. `OTA_WIFI`와 STM32 platform adapter를 분리하고 host에서 callback을 dispatch하도록 구성했습니다. 실제 host의 충전 안전 조건은 별도로 연결해야 합니다.

## 문서와 코드의 차이

Bootloader 원본 README에는 staging installer 추가 전의 설명이 남아 있었습니다. 현재 [`ap_boot_remote_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_remote_update.c)에 맞춰 이 복사본의 README를 수정했습니다.

## Storage 시험 command 설정

[`boot_config.h`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Common/boot_config.h)는 `BOOT_ENABLE_STORAGE_TEST`를 정의하지만 [`ap_boot_command.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_command.c)의 `FLASH_TEST` command는 `BOOT_ENABLE_STORAGE_RESET_TEST`로 조건부 컴파일됩니다. 제공된 기본 설정에는 후자가 정의되어 있지 않아 해당 command가 포함되지 않습니다. Storage 시험 전에 build 설정과 command 조건을 확인해야 합니다. 원본 코드는 변경하지 않았습니다.

## 보안 및 복구 범위

CRC32는 firmware 인증이 아니며 DTLS와 digital signature가 구성되어 있지 않습니다. 설치 중 기존 Application은 지워지고 rollback slot은 없습니다. Bootloader command 및 시험은 Flash를 변경할 수 있으므로 장비 시험 전에 설정과 복구 수단을 확인해야 합니다.
