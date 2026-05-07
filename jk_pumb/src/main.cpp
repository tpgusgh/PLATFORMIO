#include <Arduino.h>

// ===== DFR0523 디지털 페리스타틱 펌프 제어 =====
// 배선:
//   DFR0523 빨강(VCC) -> 12V 외부전원 +
//   DFR0523 검정(GND) -> 12V 외부전원 - (ESP32 GND와 공통)
//   DFR0523 노랑(신호) -> ESP32 GPIO (PUMP_PIN)
//
// 사용법 (시리얼 모니터 115200 baud):
//   1~5 입력 -> 해당 속도 단계로 펌프 ON (계속 동작)
//   0 입력   -> 펌프 OFF (정지)

const int PUMP_PIN       = 25;    // 펌프 제어핀 (PWM 가능 핀)
const int PWM_CHANNEL    = 0;     // ESP32 LEDC 채널
const int PWM_FREQ       = 1000;  // PWM 주파수 (Hz)
const int PWM_RESOLUTION = 8;     // 0~255

// 속도 단계별 PWM 값 (0단계=정지, 1~5단계=동작)
const int SPEED_TABLE[6] = {
  0,    // 0단계: 정지
  80,   // 1단계: 약 31%
  120,  // 2단계: 약 47%
  160,  // 3단계: 약 63%
  210,  // 4단계: 약 82%
  255   // 5단계: 100% (최대)
};

int currentLevel = 0;  // 시작은 OFF

void applySpeed(int level) {
  ledcWrite(PWM_CHANNEL, SPEED_TABLE[level]);
}

void setup() {
  Serial.begin(115200);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PUMP_PIN, PWM_CHANNEL);
  applySpeed(0);

  Serial.println("DFR0523 펌프 제어 시작");
  Serial.println("1~5 입력 -> 속도 단계로 ON / 0 입력 -> OFF");
}

void loop() {
  if (Serial.available() > 0) {
    int input = Serial.parseInt();
    while (Serial.available() > 0) Serial.read();  // 버퍼 비우기

    if (input >= 0 && input <= 5) {
      currentLevel = input;
      applySpeed(currentLevel);

      if (currentLevel == 0) {
        Serial.println("펌프 OFF");
      } else {
        Serial.printf("펌프 ON (속도 %d단계, PWM=%d)\n",
                      currentLevel, SPEED_TABLE[currentLevel]);
      }
    } else {
      Serial.println("0~5 사이의 값을 입력하세요");
    }
  }
}
