#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

// =======================
// 📌 TFT 핀 설정 (VSPI)
// =======================
#define TFT_CS   5
#define TFT_DC   27
#define TFT_RST  26

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_MISO 19  // TFT는 읽기 필요 없으면 안 써도 됨

TFT_eSPI tft = TFT_eSPI();
uint16_t lineBuffer[480]; // 한 줄 버퍼

// =======================
// 📌 SD 핀 설정 (HSPI 독립)
// =======================
#define SD_CS    15
#define SD_MOSI  13
#define SD_MISO  12
#define SD_SCLK  14

SPIClass sdSPI(HSPI); // SD 전용 SPI

char filename[64];

// =======================
// ⚙️ 초기화
// =======================
void setup() {
  Serial.begin(115200);
  Serial.println("=== ILI9488 + RAW 재생 시작 ===");

  // TFT 초기화
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("TFT 초기화 완료!");

  // SD 초기화
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  if (!SD.begin(SD_CS, sdSPI, 4000000)) {
    Serial.println("❌ SD 초기화 실패!");
    tft.println("SD 초기화 실패!");
    while (1);
  }

  Serial.println("✅ SD 카드 연결 성공!");
  tft.println("SD 카드 연결 성공!");
}

// =======================
// 🎨 RAW 파일 한 프레임 그리기
// =======================
void drawRaw(const char* filename) {
  File rawFile = SD.open(filename, FILE_READ);
  if (!rawFile) {
    Serial.printf("❌ RAW 열기 실패: %s\n", filename);
    return;
  }

  for (int y = 0; y < 320; y++) {
    if (rawFile.read((uint8_t*)lineBuffer, 480*2) != 480*2) {
      Serial.println("❌ RAW 읽기 실패");
      break;
    }
    tft.pushImage(0, y, 480, 1, lineBuffer);
  }

  rawFile.close();
}

// =======================
// ▶️ 루프
// =======================
void loop() {
  const int startFrame = 1;
  const int endFrame = 250;

  for (int i = startFrame; i <= endFrame; i++) {
    sprintf(filename, "/video/frame_%04d.raw", i);
    if (SD.exists(filename)) {
      Serial.printf("▶️ 재생 중: %s\n", filename);
      drawRaw(filename);
    } else {
      Serial.printf("⚠️ 프레임 %d 없음\n", i);
      break;
    }
  }
}
