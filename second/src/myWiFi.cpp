#include "myWiFi.h"

// 전역 WiFi 인스턴스
MyWiFi myWiFi;

MyWiFi::MyWiFi() {
    // 생성자 - 특별한 초기화 불필요
}

bool MyWiFi::connect(const char* ssid, const char* password) {
    if (ssid == nullptr) {
        Serial.println("[WiFi] Error: SSID cannot be null");
        return false;
    }
    
    Serial.printf("[WiFi] Connecting to: %s\n", ssid);
    
    // WiFi 모드 설정
    WiFi.mode(WIFI_STA);
    
    // 연결 시작
    WiFi.begin(ssid, password);
    
    // 연결 대기 (10초)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("[WiFi] Connected successfully!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        Serial.println();
        Serial.println("[WiFi] Connection failed!");
        return false;
    }
}

bool MyWiFi::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

String MyWiFi::getIPAddress() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}
