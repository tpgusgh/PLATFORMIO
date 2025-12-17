#include <Arduino.h>

// =====================
// 핀 설정
// =====================
#define LEFT_EN    18
#define LEFT_IN1   26
#define LEFT_IN2   27

#define RIGHT_EN   5
#define RIGHT_IN1  21
#define RIGHT_IN2  22

// =====================
// 엔코더 핀
// =====================
#define ENC_L_A 34
#define ENC_L_B 32
#define ENC_R_A 33
#define ENC_R_B 25

// =====================
// 부저 핀
// =====================
#define BUZZER_PIN 4

volatile long enc_left = 0;
volatile long enc_right = 0;

// =====================
// PWM 설정
// =====================
#define PWM_FREQ 5000
#define PWM_RES  8
#define PWM_CH_L 0
#define PWM_CH_R 1

// =====================
// CMD 타임아웃
// =====================
#define CMD_TIMEOUT_MS 500
unsigned long last_cmd_ms = 0;

// =====================
// 엔코더 ISR
// =====================
void IRAM_ATTR isr_enc_l() {
  bool a = digitalRead(ENC_L_A);
  bool b = digitalRead(ENC_L_B);
  enc_left += (a == b) ? 1 : -1;
}

void IRAM_ATTR isr_enc_r() {
  bool a = digitalRead(ENC_R_A);
  bool b = digitalRead(ENC_R_B);
  enc_right += (a == b) ? 1 : -1;
}

// =====================
// 모터 제어
// =====================
void setMotor(int pwm_ch, int in1, int in2, float v) {
  v = constrain(v, -1.0f, 1.0f);
  int pwm = (int)(fabsf(v) * 255.0f);

  if (v > 0.01f) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (v < -0.01f) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    pwm = 0;
  }

  ledcWrite(pwm_ch, pwm);
}

void stopAll() {
  setMotor(PWM_CH_L, LEFT_IN1, LEFT_IN2, 0.0f);
  setMotor(PWM_CH_R, RIGHT_IN1, RIGHT_IN2, 0.0f);
}

// =====================
// setup
// =====================
void setup() {
  Serial.begin(115200);

  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  ledcSetup(PWM_CH_L, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(LEFT_EN, PWM_CH_L);
  ledcAttachPin(RIGHT_EN, PWM_CH_R);

  pinMode(ENC_L_A, INPUT_PULLUP);
  pinMode(ENC_L_B, INPUT_PULLUP);
  pinMode(ENC_R_A, INPUT_PULLUP);
  pinMode(ENC_R_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_L_A), isr_enc_l, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_A), isr_enc_r, CHANGE);

  last_cmd_ms = millis();

  Serial.println("READY");
}

// =====================
// loop
// =====================
void loop() {
  // ----- CMD 처리 -----
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    s.replace("\r", "");

    // ✅ CMD 1 / CMD 0 → 부저 제어
    if (s == "CMD 1") {
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("BUZZER ON");
    }
    else if (s == "CMD 0") {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("BUZZER OFF");
    }
    // ✅ CMD vL vR → 모터 제어
    else if (s.startsWith("CMD")) {
      float vl, vr;
      if (sscanf(s.c_str(), "CMD %f %f", &vl, &vr) == 2) {
        setMotor(PWM_CH_L, LEFT_IN1, LEFT_IN2, vl);
        setMotor(PWM_CH_R, RIGHT_IN1, RIGHT_IN2, vr);
        last_cmd_ms = millis();
        Serial.printf("MOTOR %.2f %.2f\n", vl, vr);
      }
    }
  }

  // ----- 안전 정지 -----
  if (millis() - last_cmd_ms > CMD_TIMEOUT_MS) {
    stopAll();
  }

  // ----- 엔코더 출력 -----
  static unsigned long last_enc = 0;
  if (millis() - last_enc > 100) {
    last_enc = millis();
    noInterrupts();
    long l = enc_left;
    long r = enc_right;
    interrupts();
    Serial.printf("ENC %ld %ld\n", l, r);
  }
}
