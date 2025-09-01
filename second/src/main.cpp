#include <Arduino.h>
#include <WiFi.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi Connected");

  Serial.println(WiFi.localIP());
  
}

void loop() {
  
}
