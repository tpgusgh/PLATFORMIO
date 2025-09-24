#ifndef MYWIFI_H
#define MYWIFI_H

#include <WiFi.h>

// 간단한 WiFi 관리 클래스
class MyWiFi {
public:
    MyWiFi();
    
    // WiFi 연결
    bool connect(const char* ssid, const char* password);
    
    // 연결 상태 확인
    bool isConnected();
    
    // IP 주소 가져오기
    String getIPAddress();
};

// 전역 WiFi 인스턴스
extern MyWiFi myWiFi;

#endif // MYWIFI_H