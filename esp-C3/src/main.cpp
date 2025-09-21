#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ===== 핀 정의 =====
#define TOUCH_PIN 16
#define PIEZO_PIN 17
#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   22

// TFT 객체
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===== 피에조 (PWM/LEDC) 설정 =====
#define PIEZO_CHANNEL 0
#define PIEZO_RESOLUTION 8

bool touched = false;

// ===== 피에조 초기화 =====
void setupPiezo() {
  ledcSetup(PIEZO_CHANNEL, 2000, PIEZO_RESOLUTION);
  ledcAttachPin(PIEZO_PIN, PIEZO_CHANNEL);
}

void playTone(int freq, int duration) {
  ledcWriteTone(PIEZO_CHANNEL, freq);
  delay(duration);
  ledcWriteTone(PIEZO_CHANNEL, 0);
  delay(20);
}

void playHappySound() {
  int melody[] = { 523, 659, 784, 1047 }; // 도-미-솔-도
  int duration[] = { 200, 200, 200, 400 };
  for (int i = 0; i < 4; i++) {
    playTone(melody[i], duration[i]);
  }
}

// ===== LCD 표시 함수 =====
void drawCatFace(bool happy) {
  tft.fillScreen(ST77XX_BLACK); // 배경을 검정색으로

  // 눈 (흰색 직사각형)
  tft.fillRoundRect(35, 40, 20, 30, 5, ST77XX_WHITE); // 왼쪽 눈
  tft.fillRoundRect(85, 40, 20, 30, 5, ST77XX_WHITE); // 오른쪽 눈

  // 입
  if (happy) {
    // 웃는 입 (살짝 곡선)
    for (int x = 60; x <= 80; x++) {
      int y = 90 + (int)(sin((x - 60) * 3.14 / 20) * 5);
      tft.drawPixel(x, y, ST77XX_WHITE);
    }
  } else {
    // 평상시 입 (작은 선)
    tft.drawLine(65, 90, 75, 90, ST77XX_WHITE);
  }
}

// ===== 하트 그리기 함수 =====
void drawHeart(int x0, int y0, int size, uint16_t color) {
  tft.fillTriangle(x0, y0, x0 - size, y0 + size, x0 + size, y0 + size, color);
  tft.fillCircle(x0 - size / 2, y0, size / 2, color);
  tft.fillCircle(x0 + size / 2, y0, size / 2, color);
}

// ===== 하트 임팩트 효과 =====
void drawHeartImpact() {
  int size = 10;

  // 하트 표시
  drawHeart(30, 20, size, ST77XX_RED);   // 왼쪽 1개
  drawHeart(90, 20, size, ST77XX_RED);   // 오른쪽 위
  drawHeart(120, 70, size, ST77XX_RED);  // 오른쪽 아래
  playHappySound(); 

  delay(600); // 잠깐 표시

  // 다시 지우기 (배경색으로)
  drawHeart(30, 20, size, ST77XX_BLACK);
  drawHeart(90, 20, size, ST77XX_BLACK);
  drawHeart(120, 70, size, ST77XX_BLACK);
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  setupPiezo();

  // TFT 초기화
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(1);

  drawCatFace(false); // 기본 얼굴
}

void loop() {
  bool currentTouch = digitalRead(TOUCH_PIN) == HIGH;

  // 상태 변화 감지
  if (currentTouch && !touched) {
    touched = true;

    drawCatFace(true);     // 웃는 얼굴
    drawHeartImpact();     // 하트 임팩트
         // 소리 재생

  } else if (!currentTouch && touched) {
    touched = false;
    drawCatFace(false);    // 기본 얼굴 복원
  }

  delay(50); // 너무 빠른 반복 방지
}
