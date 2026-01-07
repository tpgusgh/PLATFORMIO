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

int ch1 = 45;
int ch2 = 46;   // 시작부터 다르게

unsigned long rfStartTime = 0;
unsigned long rfOnSeconds = 0;

/* ===== OLED ===== */
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("NRF CONST CARRIER");
  display.println("----------------");

  display.print("STATE : ");
  display.println(rfEnabled ? "ON" : "OFF");

  display.print("TIME  : ");
  display.print(rfOnSeconds);
  display.println(" s");

  display.print("NRF1 CH: ");
  display.println(ch1);

  display.print("NRF2 CH: ");
  display.println(ch2);

  // 채널 바 (NRF1 기준)
  int bar = map(ch1, 0, 79, 0, 120);
  display.drawRect(4, 50, 120, 10, SSD1306_WHITE);
  display.fillRect(4, 50, bar, 10, SSD1306_WHITE);

  display.display();
}

/* ===== RF 제어 ===== */
void startRF() {
  Serial.println("[RF] START CONST CARRIER");

  rfStartTime = millis();

  radio1.powerUp();
  radio2.powerUp();

  delay(5); // RF 안정화

  radio1.setChannel(ch1);
  radio2.setChannel(ch2);

  radio1.startConstCarrier(RF24_PA_MAX, ch1);
  radio2.startConstCarrier(RF24_PA_MAX, ch2);
}


void stopRF() {
  Serial.println("[RF] STOP CONST CARRIER");
  radio1.stopConstCarrier();
  radio2.stopConstCarrier();
}

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 LOLIN D32 | NRF CONST CARRIER (BTN + DUAL) ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // ESP32 내부 RF OFF (자기 자신만)
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  // OLED
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  updateOLED();

  /* ===== NRF #1 (VSPI) ===== */
  vspi = new SPIClass(VSPI);
  vspi->begin(18, 23, 19, 5); // SCK, MISO, MOSI, CS
  if (radio1.begin(vspi)) {
    Serial.println("[NRF1] OK");
    radio1.setAutoAck(false);
    radio1.stopListening();
    radio1.setRetries(0, 0);
    radio1.setPALevel(RF24_PA_MAX, true);
    radio1.setDataRate(RF24_2MBPS);
    radio1.setCRCLength(RF24_CRC_DISABLED);
  } else {
    Serial.println("[NRF1] FAIL");
  }

  /* ===== NRF #2 (HSPI) ===== */
  hspi = new SPIClass(HSPI);
  hspi->begin(14, 32, 13, 4); // SCK, MISO=32, MOSI, CS
  if (radio2.begin(hspi)) {
    Serial.println("[NRF2] OK");
    radio2.setAutoAck(false);
    radio2.stopListening();
    radio2.setRetries(0, 0);
    radio2.setPALevel(RF24_PA_MAX, true);
    radio2.setDataRate(RF24_2MBPS);
    radio2.setCRCLength(RF24_CRC_DISABLED);
  } else {
    Serial.println("[NRF2] FAIL");
  }
}

/* ===== LOOP ===== */
void loop() {
  // 버튼 토글
  bool btn = digitalRead(BUTTON_PIN);
  if (lastBtn == HIGH && btn == LOW) {
    rfEnabled = !rfEnabled;
    rfEnabled ? startRF() : stopRF();
    updateOLED();
    delay(500); // debounce
  }
  lastBtn = btn;

  // ON일 때만 채널 변경 (서로 다른 랜덤 보장)
  if (rfEnabled) {
    rfOnSeconds = (millis() - rfStartTime) / 1000;

    ch1 = random(80);
    do {
      ch2 = random(80);
    } while (ch2 == ch1);

    radio1.setChannel(ch1);
    radio2.setChannel(ch2);

    delayMicroseconds(30); // PLL 안정
  }

  // OLED 주기 갱신
  static unsigned long lastOLED = 0;
  if (millis() - lastOLED > 200) {
    updateOLED();
    lastOLED = millis();
  }
}
