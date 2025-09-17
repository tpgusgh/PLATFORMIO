#include <Arduino.h>

#define SPEAKER_PIN 8   // 스피커 핀
#define BUTTON_PIN 2    // 버튼 핀

void setup() {
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 풀업 버튼 (눌리면 LOW)
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) { // 버튼 눌렸을 때
    tone(SPEAKER_PIN, 2000, 150);  // 2000Hz 150ms
    delay(10);
    noTone(SPEAKER_PIN);           // 소리 끔
  }
}
