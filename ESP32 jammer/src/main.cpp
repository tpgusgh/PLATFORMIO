#include "RF24.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_bt.h"
#include "esp_wifi.h"
#include <WiFi.h>

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
bool lastBtn = HIGH;
unsigned long pressStart = 0;
const unsigned long LONG_PRESS_MS = 600;

int ch1 = 45;
int ch2 = 46;
unsigned long rfStartTime = 0;
unsigned long rfOnSeconds = 0;

/* ===== MODE (통합 모드) ===== */
enum SystemMode {
  MODE_NRF_RANDOM,      // NRF RANDOM 모드
  MODE_NRF_SWEEP,       // NRF SWEEP 모드
  MODE_NRF_FIXED,       // NRF FIXED 모드
  MODE_WIFI_SPAM,       // WiFi 비콘 스팸 모드
  MODE_NRF_WIFI_BOTH    // NRF + WiFi 동시 모드
};

SystemMode currentMode = MODE_NRF_RANDOM;
bool nrfActive = false;     // NRF가 현재 활성화되어 있는지
bool wifiActive = false;    // WiFi가 현재 활성화되어 있는지

int sweepDir1 = 1;
int sweepDir2 = -1;
unsigned long lastSelectTick = 0;

/* ===== WiFi Beacon Spam ===== */
// Beacon Packet buffer
uint8_t beaconPacket[128] = { 
  0x80, 0x00,             // Frame Control
  0x00, 0x00,             // Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   // Destination address
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,   // Source address
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,   // BSSID
  0x00, 0x00,             // Sequence Control
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Timestamp
  0x64, 0x00,             // Beacon Interval
  0x31, 0x04,             // Capability info
  0x00                    // SSID Parameter
};

char ssids[10][32] = {
  "Kim_woo_sung_1",
  "Kim_woo_sung_2",
  "Kim_woo_sung_3",
  "Kim_woo_sung_4",
  "Kim_woo_sung_5",
  "Kim_woo_sung_6",
  "Kim_woo_sung_7",
  "Kim_woo_sung_8",
  "Kim_woo_sung_9",
  "Kim_woo_sung_10"
};

bool wifiInitialized = false;
unsigned long lastBeaconTime = 0;
int currentBeaconIndex = 0;

/* ===== 모드 이름 ===== */
const char* getModeName() {
  switch (currentMode) {
    case MODE_NRF_RANDOM:   return "NRF RANDOM";
    case MODE_NRF_SWEEP:    return "NRF SWEEP";
    case MODE_NRF_FIXED:    return "NRF FIXED";
    case MODE_WIFI_SPAM:    return "WIFI SPAM";
    case MODE_NRF_WIFI_BOTH: return "NRF+WIFI";
    default: return "UNKNOWN";
  }
}

/* ===== OLED 업데이트 ===== */
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("HYUNHO's NRF+WIFI");
  display.println("----------------");

  display.print("MODE: ");
  display.println(getModeName());
  
  display.print("NRF: ");
  display.println(nrfActive ? "ON" : "OFF");
  
  display.print("WiFi: ");
  display.println(wifiActive ? "SPAM" : "OFF");

  if (nrfActive) {
    display.print("TIME: ");
    display.print(rfOnSeconds);
    display.println(" s");
    
    display.print("CH1: ");
    display.print(ch1);
    display.print(" CH2: ");
    display.println(ch2);
  }
  
  if (currentMode == MODE_NRF_FIXED && !nrfActive) {
    display.println("SELECTING...");
  }

  display.println("----------------");
  display.println("LONG PRESS: NEXT");
  display.println("SHORT: ON/OFF");

  display.display();
}

/* ===== WiFi 초기화 ===== */
void initWiFi() {
  if (!wifiInitialized) {
    WiFi.mode(WIFI_MODE_AP);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();
    wifiInitialized = true;
    Serial.println("[WiFi] Initialized");
  }
}

void startWiFiSpam() {
  if (!wifiInitialized) {
    initWiFi();
  }
  wifiActive = true;
  Serial.println("[WiFi] SPAM STARTED");
}

void stopWiFiSpam() {
  wifiActive = false;
  Serial.println("[WiFi] SPAM STOPPED");
}

void sendBeaconPacket() {
  if (!wifiActive) return;
  
  // Set random MAC address
  beaconPacket[10] = beaconPacket[16] = random(256);
  beaconPacket[11] = beaconPacket[17] = random(256);
  beaconPacket[12] = beaconPacket[18] = random(256);
  beaconPacket[13] = beaconPacket[19] = random(256);
  beaconPacket[14] = beaconPacket[20] = random(256);
  beaconPacket[15] = beaconPacket[21] = random(256);
  
  // Set SSID length
  int ssidLen = strlen(ssids[currentBeaconIndex]);
  beaconPacket[37] = ssidLen;
  
  // Set SSID
  for(int j = 0; j < ssidLen; j++) {
    beaconPacket[38 + j] = ssids[currentBeaconIndex][j];
  }
  
  // Send packet
  esp_wifi_80211_tx(WIFI_IF_AP, beaconPacket, sizeof(beaconPacket), false);
  
  currentBeaconIndex = (currentBeaconIndex + 1) % 10;
}

/* ===== NRF 제어 ===== */
void startNRF() {
  Serial.println("[NRF] START");
  rfStartTime = millis();
  nrfActive = true;

  radio1.powerUp();
  radio2.powerUp();
  delay(5);

  radio1.setChannel(ch1);
  radio2.setChannel(ch2);

  radio1.startConstCarrier(RF24_PA_MAX, ch1);
  radio2.startConstCarrier(RF24_PA_MAX, ch2);
}

void stopNRF() {
  Serial.println("[NRF] STOP");
  nrfActive = false;
  radio1.stopConstCarrier();
  radio2.stopConstCarrier();
  radio1.powerDown();
  radio2.powerDown();
}

void updateNRFChannels() {
  if (!nrfActive) return;
  
  switch (currentMode) {
    case MODE_NRF_RANDOM:
      ch1 = random(80);
      do { ch2 = random(80); } while (ch2 == ch1);
      radio1.setChannel(ch1);
      radio2.setChannel(ch2);
      break;

    case MODE_NRF_SWEEP:
      ch1 += sweepDir1;
      ch2 += sweepDir2;
      if (ch1 >= 79 || ch1 <= 0) sweepDir1 *= -1;
      if (ch2 >= 79 || ch2 <= 0) sweepDir2 *= -1;
      radio1.setChannel(ch1);
      radio2.setChannel(ch2);
      break;

    case MODE_NRF_FIXED:
      // 고정, 변경 없음
      break;
      
    case MODE_WIFI_SPAM:
    case MODE_NRF_WIFI_BOTH:
      // NRF가 활성화된 경우에만 채널 업데이트
      if (currentMode == MODE_NRF_WIFI_BOTH) {
        // NRF+WiFi 모드에서는 NRF 채널 업데이트
        if (currentMode == MODE_NRF_WIFI_BOTH) {
          ch1 = random(80);
          do { ch2 = random(80); } while (ch2 == ch1);
          radio1.setChannel(ch1);
          radio2.setChannel(ch2);
        }
      }
      break;
  }
}

void fixedChannelSelect() {
  if (millis() - lastSelectTick >= 1000) {
    lastSelectTick = millis();
    ch1 = (ch1 + 1) % 80;
    ch2 = (ch1 + 40) % 80;
    Serial.print("[FIXED SELECT] CH1=");
    Serial.print(ch1);
    Serial.print(" CH2=");
    Serial.println(ch2);
  }
}

/* ===== 모드 업데이트 ===== */
void updateMode() {
  // 현재 모드에 따라 NRF와 WiFi 상태 설정
  switch (currentMode) {
    case MODE_NRF_RANDOM:
      if (!nrfActive) startNRF();
      if (wifiActive) stopWiFiSpam();
      break;
      
    case MODE_NRF_SWEEP:
      if (!nrfActive) startNRF();
      if (wifiActive) stopWiFiSpam();
      break;
      
    case MODE_NRF_FIXED:
      if (!nrfActive) startNRF();
      if (wifiActive) stopWiFiSpam();
      break;
      
    case MODE_WIFI_SPAM:
      if (nrfActive) stopNRF();
      if (!wifiActive) startWiFiSpam();
      break;
      
    case MODE_NRF_WIFI_BOTH:
      if (!nrfActive) startNRF();
      if (!wifiActive) startWiFiSpam();
      break;
  }
}

void nextMode() {
  currentMode = (SystemMode)((currentMode + 1) % 5);
  Serial.print("[MODE] ");
  Serial.println(getModeName());
  updateMode();
  updateOLED();
}

void toggleCurrentMode() {
  // 현재 모드의 ON/OFF 토글
  switch (currentMode) {
    case MODE_NRF_RANDOM:
    case MODE_NRF_SWEEP:
    case MODE_NRF_FIXED:
      if (nrfActive) {
        stopNRF();
      } else {
        startNRF();
      }
      break;
      
    case MODE_WIFI_SPAM:
      if (wifiActive) {
        stopWiFiSpam();
      } else {
        startWiFiSpam();
      }
      break;
      
    case MODE_NRF_WIFI_BOTH:
      if (nrfActive || wifiActive) {
        if (nrfActive) stopNRF();
        if (wifiActive) stopWiFiSpam();
      } else {
        startNRF();
        startWiFiSpam();
      }
      break;
  }
  updateOLED();
}

/* ===== 버튼 처리 ===== */
void handleButton() {
  bool btn = digitalRead(BUTTON_PIN);

  // 버튼 눌림 시작
  if (lastBtn == HIGH && btn == LOW) {
    pressStart = millis();
  }

  // 버튼 뗌
  if (lastBtn == LOW && btn == HIGH) {
    unsigned long pressTime = millis() - pressStart;

    if (pressTime < LONG_PRESS_MS) {
      // 짧게 누름: 현재 모드 ON/OFF 토글
      toggleCurrentMode();
    } else {
      // 길게 누름: 다음 모드로 변경
      nextMode();
    }
    delay(200);
  }

  lastBtn = btn;
}

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 LOLIN D32 | NRF + WIFI INTEGRATED MODE ===");
  Serial.println("Available Modes:");
  Serial.println("  1. NRF RANDOM  - Random channel hopping");
  Serial.println("  2. NRF SWEEP   - Channel sweep up/down");
  Serial.println("  3. NRF FIXED   - Fixed channel (selects when off)");
  Serial.println("  4. WIFI SPAM   - WiFi beacon spam only");
  Serial.println("  5. NRF+WIFI    - Both NRF and WiFi active");
  Serial.println("\nControl:");
  Serial.println("  - Short press : Toggle current mode ON/OFF");
  Serial.println("  - Long press  : Switch to next mode");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(esp_random());

  // ESP32 내부 RF OFF (Bluetooth only)
  esp_bt_controller_deinit();
  
  // WiFi 모드 설정
  WiFi.mode(WIFI_MODE_AP);

  // OLED
  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
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
  
  // 초기 모드 설정
  updateMode();
  updateOLED();
  
  Serial.println("[SYSTEM] Ready");
}

/* ===== LOOP ===== */
void loop() {
  // 버튼 처리
  handleButton();

  // ===== NRF 동작 =====
  if (nrfActive) {
    rfOnSeconds = (millis() - rfStartTime) / 1000;
    updateNRFChannels();
    delayMicroseconds(20);
  } else {
    rfOnSeconds = 0;
    if (currentMode == MODE_NRF_FIXED) {
      fixedChannelSelect();
    }
  }
  
  // ===== WiFi Beacon Spam =====
  if (wifiActive) {
    if (millis() - lastBeaconTime >= 10) {
      sendBeaconPacket();
      lastBeaconTime = millis();
    }
  }

  // OLED 주기 갱신
  static unsigned long lastOLED = 0;
  if (millis() - lastOLED > 200) {
    updateOLED();
    lastOLED = millis();
  }
}