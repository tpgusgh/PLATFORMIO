#include <Arduino.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <SPIFFS.h>

TFT_eSPI tft = TFT_eSPI();  // 핀은 User_Setup.h에 정의되어 있음

// LCD 해상도 (Adafruit 1.44" ST7735R → 128x128)
#define LCD_WIDTH  128
#define LCD_HEIGHT 128

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 LCD Video Player (ST7735R)");

  // LCD 초기화
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  // SPIFFS 마운트
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed!");
    while (1);
  }
  Serial.println("SPIFFS mounted successfully!");
}

void loop() {
  // frame_0001.raw ~ frame_0100.raw 반복 재생
  for (int i = 1; i <= 100; i++) {
    char filename[32];
    sprintf(filename, "/frame_%04d.raw", i);

    fs::File f = SPIFFS.open(filename, "r");
    if (!f) {
      Serial.printf("File not found: %s\n", filename);
      continue;
    }

    // 프레임 버퍼 (RGB565, 128x128)
    static uint16_t buffer[LCD_WIDTH * LCD_HEIGHT];

    size_t bytesRead = f.read((uint8_t*)buffer, sizeof(buffer));
    f.close();

    if (bytesRead == sizeof(buffer)) {
      tft.pushImage(0, 0, LCD_WIDTH, LCD_HEIGHT, buffer);
    } else {
      Serial.printf("Frame size mismatch: %s (%d bytes)\n", filename, bytesRead);
    }

    delay(66); // 약 15fps (1000/15 ≈ 66ms)
  }
}
