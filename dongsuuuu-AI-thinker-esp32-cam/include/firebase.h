// firebase.h

#ifndef FIREBASE_H
#define FIREBASE_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>

// Firebase 설정 (실제 프로젝트에 맞게 수정 필요)
extern const char FIREBASE_HOST[];
extern const String FIREBASE_AUTH;
extern const String FIREBASE_PATH;
extern const int SSL_PORT;

// DeviceInfo 구조체 정의
struct DeviceInfo
{
    String deviceId;
    String access_time;
    String address;
    int countA;
    int countB;
    int pulseA;
    int pulseB;
    String name;
    String phone;
    String status;
    int remote;
    String call;
};

// 전역 변수 선언
extern WiFiClientSecure wifi_client;
extern HttpClient http_client;
extern DeviceInfo deviceInfo;

// Firebase 관련 함수 선언
bool initFirebase();
bool initFirebaseStream(const String &deviceId);
void handleFirebaseStream(HttpClient &streamHttp);
bool updateFirebase(const String &deviceId, bool reconnectStream);
bool addDeviceIfNotExists(const String &deviceId);
bool updateFirebaseData(const String &updateData, String &response);
String createResetJsonData(const DeviceInfo &info);

// Firebase 관련 유틸리티 함수만 선언
bool recoverMemory();

// LED 및 펄스 관련 함수 선언
void remotePulse(int remoteValue);
void blinkLED(int count);

#endif // FIREBASE_H