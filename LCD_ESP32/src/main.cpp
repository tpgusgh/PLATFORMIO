#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI(); // ILI9488 초기화
#define SD_CS 15         // 실제 모듈에 맞게 수정

char filename[64];

void drawBmp(const char *filename, int x, int y);

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // SPI 안정성을 위해 낮은 주파수로 SD 초기화
  if (!SD.begin(SD_CS, SPI, 10000000)) { // 10MHz
    Serial.println("SD 초기화 실패!");
    while(1);
  }
  Serial.println("SD 초기화 성공!");

  // BMP 파일 확인
  if (SD.exists("/video/frame_1356.bmp")) {
    Serial.println("파일 존재!");
  } else {
    Serial.println("파일 없음!");
  }

  Serial.println("동영상 재생 준비 완료!");
}

void loop() {
  int startFrame = 1356;
  int endFrame   = 1356;  // 테스트용 1장만
  for (int i = startFrame; i <= endFrame; i++) {
    sprintf(filename, "/video/frame_%04d.bmp", i);
    if (SD.exists(filename)) {
      drawBmp(filename, 0, 0);
    } else {
      Serial.printf("프레임 %d 없음!\n", i);
      break;
    }
    delay(500); // 테스트용 0.5초
  }
}

// 480x320 BMP 출력 함수
void drawBmp(const char *filename, int x, int y) {
  File bmpFile = SD.open(filename);
  if (!bmpFile) {
    Serial.println("BMP 파일 열기 실패!");
    return;
  }

  uint8_t header[54];
  bmpFile.read(header, 54);

  int bmpWidth  = header[18] | (header[19] << 8);
  int bmpHeight = header[22] | (header[23] << 8);

  if (bmpWidth != 480 || bmpHeight != 320) {
    Serial.printf("BMP 해상도 480x320 필요 (현재 %dx%d)\n", bmpWidth, bmpHeight);
    bmpFile.close();
    return;
  }

  int rowSize = ((bmpWidth * 3 + 3) & ~3);
  uint8_t rowBuffer[rowSize];

  for (int row = 0; row < bmpHeight; row++) {
    bmpFile.seek(54 + (bmpHeight - 1 - row) * rowSize);
    bmpFile.read(rowBuffer, rowSize);

    uint16_t line[bmpWidth];
    for (int col = 0; col < bmpWidth; col++) {
      uint8_t b = rowBuffer[col * 3];
      uint8_t g = rowBuffer[col * 3 + 1];
      uint8_t r = rowBuffer[col * 3 + 2];
      line[col] = tft.color565(r, g, b);
    }
    tft.pushImage(x, y + row, bmpWidth, 1, line);
  }

  bmpFile.close();
}
