#ifndef CAPTIVEPORTAL_H
#define CAPTIVEPORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"

class CaptivePortal {
private:
    AsyncWebServer* server;
    bool isAPMode;
    String ssid;
    String pass;
    
    // 파라미터 이름
    const char* PARAM_INPUT_1 = "ssid";
    const char* PARAM_INPUT_2 = "pass";
    
    // 재부팅 관련
    bool shouldReboot;
    unsigned long rebootTime;
    const unsigned long REBOOT_DELAY = 8000; // 8초 후 재부팅
    
    // WiFi 연결 실패 카운터
    int wifiFailureCount;
    const int MAX_WIFI_FAILURES = 3; // 최대 연결 실패 횟수
    const char* failureCountPath = "/wifi_failures.txt"; // 실패 카운터 저장 파일
    
    // 파일 시스템 함수들
    bool initLittleFS();
    String readFile(fs::FS &fs, const char * path);
    void writeFile(fs::FS &fs, const char * path, const char * message);
    
    // 웹 서버 설정
    void setupWebServer();
    void handleRoot(AsyncWebServerRequest *request);
    void handlePost(AsyncWebServerRequest *request);
    void handleDeviceId(AsyncWebServerRequest *request);
    void handleStaticFiles(AsyncWebServerRequest *request);
    
public:
    CaptivePortal();
    ~CaptivePortal();
    
    // 초기화 및 시작
    bool begin();
    void startAPMode();
    void stopAPMode();
    
    // 설정 관리
    void loadSettings();
    void saveSettings();
    void resetSettings();
    bool hasSettings();
    
    // 상태 확인
    bool isInAPMode();
    bool needsReboot();
    void checkReboot();
    
    // WiFi 연결 실패 처리
    void onWiFiFailure();
    void resetWiFiFailureCount();
    bool shouldStartAPMode();
    void loadFailureCount();
    void saveFailureCount();
    
    // 설정값 접근
    String getSSID();
    String getPassword();
    
    // 장치 ID 가져오기
    String getDeviceId();
    
    // 서버 처리
    void handleClient();
};

#endif // CAPTIVEPORTAL_H
