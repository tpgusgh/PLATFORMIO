#include "BluetoothSerial.h"

BluetoothSerial SerialBT;  // Bluetooth Serial 객체 생성

void setup() {
  Serial.begin(115200);               // 시리얼 모니터
  SerialBT.begin("ESP32_hyunho");    // 블루투스 이름 설정
  Serial.println("Bluetooth started! Pair your device.");
}

void loop() {
  // PC → Bluetooth (전송)
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }

  // Bluetooth → PC (수신)
  if (SerialBT.available()) {
    char incomingChar = SerialBT.read();
    Serial.write(incomingChar);

    // 간단한 응답 예시
    if (incomingChar == '1') {
      SerialBT.println("LED ON");
    } else if (incomingChar == '0') {
      SerialBT.println("LED OFF");
    }
  }
}
