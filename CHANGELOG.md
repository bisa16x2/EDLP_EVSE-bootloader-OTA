# 변경 이력

## 포트폴리오 구성

- Bootloader와 OTA package의 추적 파일을 독립 폴더로 복사하고 중첩 Git metadata 제외
- OTA 연결 설정의 Wi-Fi SSID·password와 server host를 예시 값으로 교체
- 개인·팀 기여와 원본 commit을 구분하고 시스템 구조, 코드 흐름, protocol 및 검증 문서 작성
- staging installer 추가 이전에 작성된 Bootloader README에 현재 코드와의 차이 안내
- Bootloader에서 직접 작성 코드와 주요 설정을 남기고 생성 코드, IDE 설정 및 build 보조 파일 제외
- 시험용 Application을 추가하고 CubeMX 생성 코드와 build 산출물을 제외
- 세 firmware 폴더의 역할과 이름을 문서에 반영
- staging 설치, download 완료와 설치 요청의 구분, Test Application 발췌본 설명을 현재 코드와 일치하도록 수정
- 원본 연결 설정이 포함된 `TestApp.bin`을 제외하고 재생성한 binary의 STM32CubeProgrammer 기록 주소 안내
- 최상위 README의 세 firmware 구성에 맞춰 하위 문서의 역할, 상태 전이 및 검증 범위 정리

이번 정리 과정에서는 firmware build 또는 실제 장비 시험을 수행하지 않았습니다.
