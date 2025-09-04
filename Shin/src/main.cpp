#include <WiFi.h>
#include <HTTPClient.h>

// WiFi 설정
const char* ssid = "bssm_free";
const char* password = "bssm_free";

// RGB LED 핀 설정
const int redPin = 25;
const int greenPin = 26;
const int bluePin = 27;

void setup() {
  Serial.begin(115200);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);


  ledcAttachPin(redPin, 0);
  ledcAttachPin(greenPin, 1);
  ledcAttachPin(bluePin, 2);
  ledcSetup(0, 5000, 8); // 채널0, 주파수 5kHz, 8비트 해상도
  ledcSetup(1, 5000, 8);
  ledcSetup(2, 5000, 8);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi Connected!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://10.150.1.196:8000/color"); 
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);


      int rIndex = payload.indexOf("\"red\":") + 6;
      int gIndex = payload.indexOf("\"green\":") + 8;
      int bIndex = payload.indexOf("\"blue\":") + 7;

      int red = payload.substring(rIndex, payload.indexOf(",", rIndex)).toInt();
      int green = payload.substring(gIndex, payload.indexOf(",", gIndex)).toInt();
      int blue = payload.substring(bIndex, payload.indexOf("}", bIndex)).toInt();

      
      ledcWrite(0, red);   // 0~255
      ledcWrite(1, green);
      ledcWrite(2, blue);
    } else {
      Serial.println("HTTP Request Failed!");
    }
    http.end();
  }

  delay(2000); // 2초마다 서버에서 색 갱신
}
