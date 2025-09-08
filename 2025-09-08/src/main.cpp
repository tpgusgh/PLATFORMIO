#include <Arduino.h>
#include <WiFi.h>
#include "myfileSystem.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const int dayLightOffset = 0;
void syncNTP(){}

void setup() {
  Serial.begin(115200);

  initLittleFS();
  writeLittleFS();
  readLittleFS();
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
