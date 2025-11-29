ESP32PulseFly: 단일 BIN(펌웨어+파일시스템) 생성 및 사용 가이드

이 프로젝트는 빌드 시 자동으로 펌웨어(bootloader, partitions, app)와 LittleFS 이미지를 병합하여 하나의 `combined.bin`을 생성합니다. 추가 명령 없이 IDE의 빌드 버튼을 눌러도 동작합니다.

준비 사항
- PlatformIO (IDE 또는 CLI)
- 보드: `esp32dev`
- 파일시스템: LittleFS
- 파티션: `partitions_4mb.csv`

이미 설정되어 있는 항목
- `platformio.ini`에 스크립트 연동: `extra_scripts = post:scripts/merge_bins.py`
- 병합 스크립트: `scripts/merge_bins.py`

단일 BIN 생성 방법
IDE에서
1. 빌드 아이콘(기본 Build)을 클릭합니다.
2. 자동으로 펌웨어 빌드 → LittleFS 이미지 생성(필요 시) → 병합이 수행됩니다.

CLI에서
```
# 펌웨어 빌드 (필수)
pio run -e esp32cam_serial
# 파일시스템 이미지 (필요 시 자동 실행되지만, 수동 실행도 가능)
pio run -e esp32cam_serial -t buildfs
```

생성물 위치
- `ESP32PulseFly/.pio/build/esp32cam_serial/combined.bin`

참고(구성 파일들)
- 부트로더: `.pio/build/esp32cam_serial/bootloader.bin`
- 파티션: `.pio/build/esp32cam_serial/partitions.bin`
- 앱(펌웨어): `.pio/build/esp32cam_serial/firmware.bin`
- 파일시스템: `.pio/build/esp32cam_serial/littlefs.bin`

단일 BIN 플래싱 방법(시리얼)
```
esptool.py --chip esp32 --port COM<번호> --baud 921600 \
  write_flash -z 0x0 .pio/build/esp32cam_serial/combined.bin
```
- 예: `COM12` 사용 시 `--port COM12`
- 오프셋은 항상 `0x0` (병합 이미지 기준)

OTA 관련
- OTA는 앱 파티션만 교체합니다. OTA 업로드에는 `combined.bin`이 아닌 `firmware.bin`을 사용하세요.

파티션/오프셋 참고
- 부트로더: `0x1000`
- 파티션 테이블: `0x8000`
- 앱(펌웨어): `0x10000`
- LittleFS: `partitions_4mb.csv`에서 읽은 오프셋(기본 `0x310000`)

유용한 팁
- `data/` 폴더의 정적 파일을 변경한 경우, 빌드시 자동으로 LittleFS 이미지가 갱신됩니다.
- `combined.bin` 하나만으로 공장 초기화/완전 배포가 가능합니다.


