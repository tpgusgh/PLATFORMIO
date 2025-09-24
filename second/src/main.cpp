#include <Arduino.h>
#include "myFileSystem.h"
#include "myWiFi.h"
#include "captiveportal.h"

// CaptivePortal 인스턴스 생성
CaptivePortal captivePortal;

void setup()
{
  // Initialize serial communication (ESP32-S3 USB CDC)
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  Serial.println("=== ESP32S3 WiFi Setup ===");

  // CaptivePortal 초기화 및 시작
  if (captivePortal.begin())
  {
    Serial.println("CaptivePortal started - AP mode active");
    Serial.println("Connect to 'ESP32S3 WiFi Setup' network to configure WiFi");
  }
  else
  {
    Serial.println("CaptivePortal not needed - WiFi credentials found");
    
    // 기존 WiFi 연결 로직
    // Initialize LittleFS filesystem
    if (!MyFileSystem::initialize())
    {
      Serial.println("Failed to initialize filesystem!");
      return;
    }

    // Print filesystem tree structure
    MyFileSystem::printTree();

    // Load and display JSON configuration
    if (MyFileSystem::loadConfig())
    {
      MyFileSystem::printConfig();

      // Get WiFi credentials from config
      String ssid = MyFileSystem::getWiFiSSID();
      String password = MyFileSystem::getWiFiPassword();

      if (ssid.length() > 0)
      {
        Serial.println("=== WiFi Connection ===");
        Serial.printf("SSID: %s\n", ssid.c_str());

        // Connect to WiFi
        if (myWiFi.connect(ssid.c_str(), password.c_str()))
        {
          Serial.println("WiFi connection successful!");
          Serial.printf("IP Address: %s\n", myWiFi.getIPAddress().c_str());
          // WiFi 연결 성공 시 실패 카운터 리셋
          captivePortal.resetWiFiFailureCount();
        }
        else
        {
          Serial.println("WiFi connection failed!");
          // WiFi 연결 실패 시 카운터 증가
          captivePortal.onWiFiFailure();
        }
      }
      else
      {
        Serial.println("No WiFi SSID found in config!");
      }
    }
    else
    {
      Serial.println("Failed to load config file!");
    }

    // Print filesystem information
    MyFileSystem::printInfo();
  }
}

void loop()
{
  // CaptivePortal이 활성화된 경우 재부팅 체크
  if (captivePortal.isInAPMode())
  {
    captivePortal.checkReboot();
  }
  else
  {
    // 기존 WiFi 모니터링 로직
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000)
    {
      if (myWiFi.isConnected())
      {
        Serial.printf("WiFi Status: Connected (IP: %s)\n", myWiFi.getIPAddress().c_str());
        // WiFi 연결 상태 확인 시 실패 카운터 리셋
        captivePortal.resetWiFiFailureCount();
      }
      else
      {
        Serial.println("WiFi Status: Disconnected");
        // WiFi 연결 끊김 시 실패 카운터 증가
        captivePortal.onWiFiFailure();
        
        // 최대 실패 횟수에 도달하면 재부팅하여 AP 모드로 전환
        if (captivePortal.shouldStartAPMode()) {
          Serial.println("Maximum WiFi failures reached. Rebooting to start AP mode...");
          delay(2000);
          ESP.restart();
        }
      }
      lastWiFiCheck = millis();
    }
  }

  delay(1000);
}
