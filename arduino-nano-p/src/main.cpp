#include <Arduino.h>

#define PIEZO_PIN 8     // 피에조 부저 핀
#define BUTTON_PIN 2    // 버튼 핀 (디지털 핀 2번 사용)

void setup() {
  pinMode(PIEZO_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 버튼을 풀업 입력으로 설정 (눌리면 LOW)
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {   // 버튼 눌림 감지
    tone(PIEZO_PIN, 1000, 400); // 400ms 동안 울림
    delay(450);
    tone(PIEZO_PIN, 1200, 400);
    delay(450);
  }
}
