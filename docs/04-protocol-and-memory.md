# 통신과 메모리

## UART Bootloader protocol

[`boot_protocol.h`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Middleware/boot_protocol.h)와 [`boot_protocol.c`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Middleware/boot_protocol.c)를 기준으로 합니다.

```text
SOF AA 55 | Command 1 byte | Length LE 2 bytes | Data 0..512 bytes | CRC16 LE
```

CRC16은 `Command + Length + Data`에 적용합니다. 주요 명령은 `HELLO`, `GET_VERSION`, `START_UPDATE`, `DATA`, `END_UPDATE`, `ABORT`, `RUN_APP`이며 응답으로 ACK/NACK을 사용합니다. DATA의 첫 4바이트는 offset이므로 한 packet의 firmware data는 최대 508바이트입니다.

## Flash와 Backup SRAM

| 영역 | 시작 주소 | 크기 |
| --- | --- | --- |
| Bootloader | `0x08000000` | 128 KiB |
| Application | `0x08020000` | 896 KiB |
| Staging | `0x08100000` | 896 KiB |
| Reserved | `0x081E0000` | 128 KiB |

Backup SRAM의 `+0`, `+4`는 Boot request, `+8`부터 `+20`은 staging metadata, `+24`는 설치 결과입니다. OTA package는 `+28`, `+48`에 Wakaama MID journal을 사용합니다. 양쪽 설정은 [`boot_config.h`](../firmware/EVSE_prev_bootloader/EVSE_BOOT/Common/boot_config.h)와 [`boot_request_config.h`](../firmware/bootloader-package/EVSE_APP/PACKAGE/OTA_WIFI/COMMON/boot_request_config.h)에서 확인할 수 있습니다.

무선 경로는 ESP8266 UART를 통해 UDP를 사용하며, LwM2M Firmware Object `/5`와 CoAP Block2 download를 연결합니다. CRC32는 오류 검출 수단이며 송신자의 신원을 인증하지 않습니다.
