#include <WiFi.h>
#include <HTTPClient.h>

// WiFi
const char* ssid = "bssm_free";
const char* password = "bssm_free";

// GPIO 핀 매핑
// 상단 LED
const int R1 = 25, G1 = 26, BL1 = 27;
// 중간 LED
const int R2 = 14, G2 = 12, B2 = 13;
// 하단 LED
const int R3 = 19, G3 = 18, B3 = 5;

// PWM 채널 (0~15 사용 가능)
const int CH_R1 = 0, CH_G1 = 1, CH_B1 = 2;
const int CH_R2 = 3, CH_G2 = 4, CH_B2 = 5;
const int CH_R3 = 6, CH_G3 = 7, CH_B3 = 8;


const int ON = 255;
const int OFF = 0;

String serverUrl = "http://10.150.1.196:8000/signal"; // 주소

void pwmAttachAll() {
  ledcAttachPin(R1, CH_R1); ledcAttachPin(G1, CH_G1); ledcAttachPin(BL1, CH_B1);
  ledcAttachPin(R2, CH_R2); ledcAttachPin(G2, CH_G2); ledcAttachPin(B2, CH_B2);
  ledcAttachPin(R3, CH_R3); ledcAttachPin(G3, CH_G3); ledcAttachPin(B3, CH_B3);
  // 5kHz, 8-bit
  ledcSetup(CH_R1, 5000, 8); ledcSetup(CH_G1, 5000, 8); ledcSetup(CH_B1, 5000, 8);
  ledcSetup(CH_R2, 5000, 8); ledcSetup(CH_G2, 5000, 8); ledcSetup(CH_B2, 5000, 8);
  ledcSetup(CH_R3, 5000, 8); ledcSetup(CH_G3, 5000, 8); ledcSetup(CH_B3, 5000, 8);
}

void allOff() {
  ledcWrite(CH_R1, OFF); ledcWrite(CH_G1, OFF); ledcWrite(CH_B1, OFF);
  ledcWrite(CH_R2, OFF); ledcWrite(CH_G2, OFF); ledcWrite(CH_B2, OFF);
  ledcWrite(CH_R3, OFF); ledcWrite(CH_G3, OFF); ledcWrite(CH_B3, OFF);
}

void showRedTop() {           // 빨강
  allOff();
  ledcWrite(CH_R1, ON);
}

void showYellowMiddle() {     // 노랑
  allOff();
  ledcWrite(CH_R2, ON);
  ledcWrite(CH_G2, ON);
}

void showBlueBottom() {       //  파랑
  allOff();
  ledcWrite(CH_B3, ON);
}

char parseSignalFromJson(const String& json) {
  int i = json.indexOf("\"signal\"");
  if (i < 0) return 'X';
  int q1 = json.indexOf('"', i + 8);
  int q2 = json.indexOf('"', q1 + 1);
  int q3 = json.indexOf('"', q2 + 1);
  if (q2 < 0 || q3 < 0) return 'X';
  if (q3 == q2 + 2) {
    return json[q2 + 1];
  }
  return 'X';
}

void setup() {
  Serial.begin(115200);

  pinMode(R1, OUTPUT); pinMode(G1, OUTPUT); pinMode(BL1, OUTPUT);
  pinMode(R2, OUTPUT); pinMode(G2, OUTPUT); pinMode(B2, OUTPUT);
  pinMode(R3, OUTPUT); pinMode(G3, OUTPUT); pinMode(B3, OUTPUT);
  pwmAttachAll();
  allOff();

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected, IP: " + WiFi.localIP().toString());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    int code = http.GET();
    if (code > 0) {
      String payload = http.getString();
      Serial.println("RX: " + payload);
      char sig = parseSignalFromJson(payload);
      switch (sig) {
        case 'R': showRedTop(); break;
        case 'G': showYellowMiddle(); break;
        case 'B': showBlueBottom(); break;
        default:  allOff(); break; 
      }
    } else {
      Serial.printf("HTTP GET failed: %d\n", code);
    }
    http.end();
  }
  delay(1000);
}
