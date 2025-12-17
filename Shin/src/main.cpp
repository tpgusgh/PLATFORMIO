#include <WiFi.h>
#include <HTTPClient.h>

// WiFi 설정
const char* ssid = "bssm_";
const char* password = "xogus486!!";

// GPIO 핀 매핑
const int R1 = 25, G1 = 26, BL1 = 27;   // 빨강
const int R2 = 14;                      // 노랑(단색 LED)
const int R3 = 19, G3 = 18, B3 = 5;     // 초록

// PWM 채널 (ESP32)
const int CH_R1 = 0, CH_G1 = 1, CH_B1 = 2;
const int CH_R3 = 6, CH_G3 = 7, CH_B3 = 8;

const int ON = 255;
const int OFF = 0;

String serverUrl = "https://bssm.heodongun.com/esp";

// PWM 초기화
void pwmAttachAll() {
  // 빨강
  ledcSetup(CH_R1, 5000, 8); ledcAttachPin(R1, CH_R1);
  ledcSetup(CH_G1, 5000, 8); ledcAttachPin(G1, CH_G1);
  ledcSetup(CH_B1, 5000, 8); ledcAttachPin(BL1, CH_B1);

  // 초록 (RGB)
  ledcSetup(CH_R3, 5000, 8); ledcAttachPin(R3, CH_R3);
  ledcSetup(CH_G3, 5000, 8); ledcAttachPin(G3, CH_G3);
  ledcSetup(CH_B3, 5000, 8); ledcAttachPin(B3, CH_B3);
}

// 모든 LED 끄기
void allOff() {
  // PWM LED OFF
  ledcWrite(CH_R1, OFF); ledcWrite(CH_G1, OFF); ledcWrite(CH_B1, OFF);
  ledcWrite(CH_R3, OFF); ledcWrite(CH_G3, OFF); ledcWrite(CH_B3, OFF);

  // 단색 LED OFF
  digitalWrite(R2, LOW);
}

void showRedTop() {          
  allOff();
  ledcWrite(CH_R1, ON);
}

void showYellowMiddle() {    
  allOff();
  digitalWrite(R2, HIGH);  // GPIO14 단색 LED만 ON
}

void showGreenBottom() {     
  allOff();
  ledcWrite(CH_G3, ON);
}

// JSON에서 signal 파싱
char parseSignalFromJson(const String& json) {
  int start = json.indexOf("\"signal\":\"");
  if (start < 0) return 'X';
  start += 10;
  if (start >= json.length()) return 'X';
  char sig = toupper(json[start]); 
  return sig;
}

void setup() {
  Serial.begin(115200);

  // 단색 LED
  pinMode(R2, OUTPUT);

  // PWM 핀
  pinMode(R1, OUTPUT); pinMode(G1, OUTPUT); pinMode(BL1, OUTPUT);
  pinMode(R3, OUTPUT); pinMode(G3, OUTPUT); pinMode(B3, OUTPUT);

  pwmAttachAll();
  allOff();

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nWiFi 연결됨, IP: " + WiFi.localIP().toString());
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
        case 'Y': showYellowMiddle(); break;  // GPIO14만 ON
        case 'G': showGreenBottom(); break;
        default:  allOff(); break; 
      }
    } else {
      Serial.printf("HTTP GET failed: %d\n", code);
    }
    http.end();
  }

  delay(500); 
}
