#include "RF24.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_bt.h"
#include "esp_wifi.h"

/* ===== OLED ===== */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ===== 버튼 ===== */
#define BUTTON_PIN 33   // 버튼 -> GND (INPUT_PULLUP)

/* ===== SPI ===== */
SPIClass *vspi = nullptr; // NRF #1
SPIClass *hspi = nullptr; // NRF #2

/* ===== RF24 ===== */
RF24 radio1(27, 5, 16000000); // NRF #1 (VSPI)
RF24 radio2(26, 4, 16000000); // NRF #2 (HSPI)

/* ===== 상태 ===== */
bool rfEnabled = false;
bool lastBtn = HIGH;

unsigned long pressStart = 0;
const unsigned long LONG_PRESS_MS = 600;

int ch1 = 45;
int ch2 = 46;

unsigned long rfStartTime = 0;
unsigned long rfOnSeconds = 0;

/* ===== MODE ===== */
enum Mode {
  MODE_RANDOM,
  MODE_SWEEP,
  MODE_FIXED
};
Mode mode = MODE_RANDOM;

int sweepDir1 = 1;
int sweepDir2 = -1;

/* ===== FIXED 채널 선택 ===== */
unsigned long lastSelectTick = 0;

/* ===== OLED ===== */
const char* modeName() {
  switch (mode) {
    case MODE_RANDOM: return "RANDOM";
    case MODE_SWEEP:  return "SWEEP";
    case MODE_FIXED:  return "FIXED";
    default: return "?";
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("HYUNHO's NRF CONST CARRIER");
  display.println("----------------");

  display.print("STATE : ");
  display.println(rfEnabled ? "ON" : "OFF");

  display.print("MODE  : ");
  display.println(modeName());

  if (mode == MODE_FIXED && !rfEnabled) {
    display.println("SELECTING...");
  }

  display.print("TIME  : ");
  display.print(rfOnSeconds);
  display.println(" s");

  display.print("NRF1 CH: ");
  display.println(ch1);

  display.print("NRF2 CH: ");
  display.println(ch2);

  display.display();
}

/* ===== RF 제어 ===== */
void startRF() {
  Serial.println("[RF] START");
  rfStartTime = millis();

  radio1.powerUp();
  radio2.powerUp();
  delay(5);

  radio1.setChannel(ch1);
  radio2.setChannel(ch2);

  radio1.startConstCarrier(RF24_PA_MAX, ch1);
  radio2.startConstCarrier(RF24_PA_MAX, ch2);
}

void stopRF() {
  Serial.println("[RF] STOP");
  radio1.stopConstCarrier();
  radio2.stopConstCarrier();
  radio1.powerDown();
  radio2.powerDown();
}

/* ===== 채널 업데이트 ===== */
void updateChannels() {
  switch (mode) {
    case MODE_RANDOM:
      ch1 = random(80);
      do { ch2 = random(80); } while (ch2 == ch1);
      break;

    case MODE_SWEEP:
      ch1 += sweepDir1;
      ch2 += sweepDir2;
      if (ch1 >= 79 || ch1 <= 0) sweepDir1 *= -1;
      if (ch2 >= 79 || ch2 <= 0) sweepDir2 *= -1;
      break;

    case MODE_FIXED:
      // RF ON일 때는 고정 유지
      break;
  }

  radio1.setChannel(ch1);
  radio2.setChannel(ch2);
}

/* ===== FIXED 채널 선택 (RF OFF) ===== */
void fixedChannelSelect() {
  if (millis() - lastSelectTick >= 1000) {
    lastSelectTick = millis();

    ch1 = (ch1 + 1) % 80;
    ch2 = (ch1 + 40) % 80;  // 항상 다르게

    Serial.print("[FIXED SELECT] CH1=");
    Serial.print(ch1);
    Serial.print(" CH2=");
    Serial.println(ch2);
  }
}

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 LOLIN D32 | NRF CONST CARRIER MODES ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(esp_random());

  // ESP32 내부 RF OFF (자기 자신만)
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  // OLED
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  updateOLED();

  /* NRF #1 */
  vspi = new SPIClass(VSPI);
  vspi->begin(18, 23, 19, 5);
  radio1.begin(vspi);
  radio1.setAutoAck(false);
  radio1.stopListening();
  radio1.setRetries(0, 0);
  radio1.setPALevel(RF24_PA_MAX, true);
  radio1.setDataRate(RF24_2MBPS);
  radio1.setCRCLength(RF24_CRC_DISABLED);

  /* NRF #2 */
  hspi = new SPIClass(HSPI);
  hspi->begin(14, 32, 13, 4);
  radio2.begin(hspi);
  radio2.setAutoAck(false);
  radio2.stopListening();
  radio2.setRetries(0, 0);
  radio2.setPALevel(RF24_PA_MAX, true);
  radio2.setDataRate(RF24_2MBPS);
  radio2.setCRCLength(RF24_CRC_DISABLED);
}

/* ===== LOOP ===== */
void loop() {
  bool btn = digitalRead(BUTTON_PIN);

  // 버튼 눌림 시작
  if (lastBtn == HIGH && btn == LOW) {
    pressStart = millis();
  }

  // 버튼 뗌
  if (lastBtn == LOW && btn == HIGH) {
    unsigned long pressTime = millis() - pressStart;

    if (pressTime < LONG_PRESS_MS) {
      rfEnabled = !rfEnabled;
      rfEnabled ? startRF() : stopRF();
    } else {
      mode = (Mode)((mode + 1) % 3);
      Serial.print("[MODE] ");
      Serial.println(modeName());
    }
    updateOLED();
    delay(200);
  }

  lastBtn = btn;

  // ===== 동작 =====
  if (rfEnabled) {
    rfOnSeconds = (millis() - rfStartTime) / 1000;
    updateChannels();
    delayMicroseconds(20);
  } else {
    rfOnSeconds = 0;
    if (mode == MODE_FIXED) {
      fixedChannelSelect();
    }
  }

  // OLED 주기 갱신
  static unsigned long lastOLED = 0;
  if (millis() - lastOLED > 200) {
    updateOLED();
    lastOLED = millis();
  }
}
