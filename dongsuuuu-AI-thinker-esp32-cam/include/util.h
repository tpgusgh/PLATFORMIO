// util.h
#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

// extern 선언 추가
extern Preferences preferences;

// 디버깅 출력을 위한 매크로
#define DEBUG_MODE true // true: 디버깅 정보 출력, false: IO보드 통신만

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(x, ...) Serial.printf(x, __VA_ARGS__)
#else
#define DEBUG_PRINT(x) ((void)0)
#define DEBUG_PRINTLN(x) ((void)0)
#define DEBUG_PRINTF(x, ...) ((void)0)
#endif

char *getChipID();
void syncNTPTime();
String getCurrentTimeString();

// URL 인코딩/디코딩 함수 선언
String urlEncode(String str);
String urlDecode(String str);
int hexCharToInt(char c);

// 파일 시스템 관련 함수 선언
bool writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);

String generateRandomString(int length);
void readData(uint16_t &countA, uint16_t &countB, uint16_t &pulseA, uint16_t &pulseB);
void saveData(uint16_t countA, uint16_t countB, uint16_t pulseA, uint16_t pulseB);