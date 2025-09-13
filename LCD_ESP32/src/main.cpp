#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI();  // TFT 객체 생성

#define SD_CS 15  // SD 카드 CS 핀

File bmpFile;

// 최대 128픽셀 폭까지 버퍼 사용
uint16_t rowBuffer[128];  

void drawBmp(const char *filename, int x, int y) {
  bmpFile = SD.open(filename);
  if (!bmpFile) {
    Serial.println("파일 열기 실패!");
    return;
  }

  // BMP 헤더 읽기 (54바이트)
  uint8_t header[54];
  bmpFile.read(header, 54);

  // BMP 파일 정보
  int bmpWidth  = header[18] | (header[19] << 8);
  int bmpHeight = header[22] | (header[23] << 8);
  int rowSize = ((bmpWidth * 3 + 3) & ~3); // 4바이트 배수

  if (bmpWidth > 128 || bmpHeight > 128) {
    Serial.println("BMP가 화면보다 큼!");
    bmpFile.close();
    return;
  }

  for (int row = 0; row < bmpHeight; row++) {
    int pixelIndex = 0;
    for (int col = 0; col < bmpWidth; col++) {
      uint8_t b = bmpFile.read();
      uint8_t g = bmpFile.read();
      uint8_t r = bmpFile.read();
      rowBuffer[pixelIndex++] = tft.color565(r, g, b);
    }

    // 남는 패딩 바이트 처리
    for (int i = pixelIndex * 3; i < rowSize; i++) {
      bmpFile.read();
    }

    // TFT에 한 줄 전송 (BMP는 아래->위 저장)
    tft.pushColors(rowBuffer, bmpWidth, true);
  }

  bmpFile.close();
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD 초기화 실패!");
    return;
  }

  drawBmp("/test.bmp", 0, 0);
}

void loop() {
}
