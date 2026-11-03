# Bootloader 코드 흐름

기준 경로는 `firmware/EVSE_prev_bootloader/EVSE_BOOT/`입니다. 이 Bootloader의 설계와 구현은 단독 담당했습니다.

## 부팅과 설치

1. [`App/ap_bootloader.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_bootloader.c)의 `ApBootloader_Init()`이 BSP, UART transport, storage와 update context를 초기화합니다.
2. `ApBootloader_Run()`이 USER 버튼과 Backup SRAM의 원격 update 요청을 확인합니다.
3. 원격 요청을 지운 뒤 [`App/ap_boot_remote_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_remote_update.c)의 `ApBootRemoteUpdate_Install()`이 staging metadata, image 크기, vector와 CRC32를 검사합니다.
4. 검증이 성공하면 update engine을 이용해 Application 영역에 복사하고 결과를 Backup SRAM에 기록한 뒤 Application으로 jump를 시도합니다. 설치가 실패해도 기존 Application이 유효하면 이를 실행하려고 시도합니다.
5. 그 외 버튼 요청, 유효하지 않은 Application 또는 jump 실패 등의 경우 UART command loop로 들어갑니다. 버튼 요청과 원격 요청이 동시에 있더라도 원격 요청을 확인하고 설치를 시도합니다.

## 복사본의 build 범위

이 폴더는 `Bootloader.ioc`, `CMakeLists.txt`, linker script와 직접 작성 코드를 남긴 발췌본입니다. CubeMX 생성 코드와 CMake 보조 설정을 재구성해야 하며, `main.c`의 Bootloader 초기화·실행 호출도 복원해야 합니다. 작성자는 직접 설치해 프로젝트 복원을 확인했습니다. 별도의 build 결과는 이번 정리 과정에서 확인하지 않았습니다. 자세한 내용은 [Bootloader README](../firmware/EVSE_prev_bootloader/README.md)를 참고하세요.

## UART update와 image commit

[`Middleware/boot_protocol.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Middleware/boot_protocol.c)는 byte 단위 frame과 CRC16을 처리합니다. [`App/ap_boot_command.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_command.c)는 명령을 update engine으로 전달하고 ACK/NACK을 만듭니다.

[`App/ap_boot_update.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/App/ap_boot_update.c)의 `ApBootUpdate_Start()`는 영역을 지우고, `ApBootUpdate_WriteData()`는 순차 offset과 Flash 기록을 확인합니다. 첫 8바이트는 RAM에 보관하며 `ApBootUpdate_End()`에서 크기·CRC32·vector 검증을 통과한 뒤 마지막으로 기록합니다. Flash erase/write/read-back은 [`BSP/bsp_storage.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/BSP/bsp_storage.c)가 담당합니다.

[`HW/hw_system.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/HW/hw_system.c)의 `HwSystem_IsVectorTableValid()`와 `HwSystem_JumpToVectorTable()`은 vector 검사 및 Application 실행 전 시스템 상태 정리를 담당합니다.
