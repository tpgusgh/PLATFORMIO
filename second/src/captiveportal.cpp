#include "captiveportal.h"
#include "myFileSystem.h"

CaptivePortal::CaptivePortal() {
    server = nullptr;
    isAPMode = false;
    ssid = "";
    pass = "";
    shouldReboot = false;
    rebootTime = 0;
    wifiFailureCount = 0;
}

CaptivePortal::~CaptivePortal() {
    if (server) {
        server->end();
        delete server;
    }
}

bool CaptivePortal::begin() {
    Serial.println("=== CaptivePortal::begin() START ===");
    
    // LittleFS 초기화
    if (!initLittleFS()) {
        Serial.println("Failed to initialize LittleFS");
        return false;
    }
    
    // 설정값 로드
    loadSettings();
    
    // WiFi 실패 카운터 로드
    loadFailureCount();
    
    // WiFi 설정이 없거나 연결 실패가 많으면 AP 모드 시작
    if (!hasSettings() || shouldStartAPMode()) {
        if (!hasSettings()) {
            Serial.println("No WiFi credentials found. Starting AP mode for initial setup.");
        } else {
            Serial.println("WiFi connection failed multiple times. Starting AP mode for reconfiguration.");
        }
        startAPMode();
        return true;
    }
    
    Serial.println("WiFi credentials found. AP mode not needed.");
    return false; // AP 모드가 필요하지 않음
}

void CaptivePortal::startAPMode() {
    Serial.println("=== Starting AP Mode ===");
    
    isAPMode = true;
    
    // AP 모드로 전환
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32S3 WiFi Setup", NULL);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.println("=== WiFi Setup Instructions ===");
    Serial.println("1. Connect to 'ESP32S3 WiFi Setup' network");
    Serial.println("2. Open browser and go to: http://192.168.4.1");
    Serial.println("3. Enter your WiFi credentials");
    Serial.println("================================");
    
    // 웹 서버 생성 및 설정
    if (server) {
        server->end();
        delete server;
    }
    
    server = new AsyncWebServer(80);
    setupWebServer();
    server->begin();
    
    Serial.println("Device is in AP mode. Please connect to 'ESP32S3 WiFi Setup' network.");
}

void CaptivePortal::stopAPMode() {
    Serial.println("=== Stopping AP Mode ===");
    
    if (server) {
        server->end();
        delete server;
        server = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    isAPMode = false;
}

void CaptivePortal::loadSettings() {
    Serial.println("=== Loading WiFi Settings ===");
    
    // config.txt에서 WiFi 설정 읽기
    if (MyFileSystem::loadConfig()) {
        ssid = MyFileSystem::getWiFiSSID();
        pass = MyFileSystem::getWiFiPassword();
        
        Serial.print("SSID: ");
        Serial.println(ssid);
        Serial.print("Password: ");
        Serial.println(pass);
    } else {
        Serial.println("Failed to load config.txt");
        ssid = "";
        pass = "";
    }
}

void CaptivePortal::saveSettings() {
    Serial.println("=== Saving WiFi Settings ===");
    
    // config.txt JSON 파일에만 저장
    if (MyFileSystem::loadConfig()) {
        // WiFi 설정을 JSON 구조에 맞게 저장 (기존 wifi 객체 업데이트)
        if (MyFileSystem::setWiFiCredentials(ssid.c_str(), pass.c_str())) {
            Serial.println("WiFi credentials saved to config.txt");
        } else {
            Serial.println("Failed to save config.txt");
        }
    } else {
        Serial.println("Failed to load config.txt for update");
    }
    
    Serial.println("Settings saved successfully");
    
    // WiFi 연결 실패 카운터 리셋
    resetWiFiFailureCount();
}

void CaptivePortal::resetSettings() {
    Serial.println("=== Resetting WiFi Settings ===");
    
    // config.txt에서 WiFi 설정 제거 (기존 wifi 객체 구조 유지)
    if (MyFileSystem::loadConfig()) {
        // WiFi 설정을 빈 값으로 설정
        if (MyFileSystem::setWiFiCredentials("", "")) {
            Serial.println("WiFi credentials removed from config.txt");
        } else {
            Serial.println("Failed to save config.txt");
        }
    } else {
        Serial.println("Failed to load config.txt for reset");
    }
    
    // 변수 초기화
    ssid = "";
    pass = "";
    
    Serial.println("WiFi settings reset completed");
    
    // WiFi 연결 실패 카운터 리셋
    resetWiFiFailureCount();
}

bool CaptivePortal::hasSettings() {
    return (ssid.length() > 0 && pass.length() > 0);
}

bool CaptivePortal::isInAPMode() {
    return isAPMode;
}

bool CaptivePortal::needsReboot() {
    return shouldReboot;
}

void CaptivePortal::checkReboot() {
    if (shouldReboot && (millis() - rebootTime >= REBOOT_DELAY)) {
        Serial.println("=== Scheduled reboot executing ===");
        Serial.println("Rebooting device after WiFi configuration...");
        
        shouldReboot = false;
        delay(1000);
        ESP.restart();
    }
}

String CaptivePortal::getSSID() {
    return ssid;
}

String CaptivePortal::getPassword() {
    return pass;
}


String CaptivePortal::getDeviceId() {
    // ESP32의 MAC 주소를 기반으로 고유 ID 생성
    uint64_t chipid = ESP.getEfuseMac();
    String deviceId = String((uint32_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
    deviceId.toUpperCase();
    return deviceId;
}

void CaptivePortal::handleClient() {
    // AsyncWebServer는 자동으로 처리되므로 빈 함수
    // 필요시 추가 처리 로직 구현
}

bool CaptivePortal::initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return false;
    }
    Serial.println("LittleFS mounted successfully");
    return true;
}

void CaptivePortal::setupWebServer() {
    Serial.println("=== Setting up Web Server ===");
    
    // 메인 페이지
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });
    
    // POST 요청 처리
    server->on("/", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handlePost(request);
    });
    
    // 장치 ID API
    server->on("/device-id", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleDeviceId(request);
    });
    
    // 정적 파일들
    server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleStaticFiles(request);
    });
    
    server->on("/success.html", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleStaticFiles(request);
    });
    
    // 모든 정적 파일 서빙
    server->serveStatic("/", LittleFS, "/");
}

void CaptivePortal::handleRoot(AsyncWebServerRequest *request) {
    Serial.println("Received request for /");
    if (!LittleFS.exists("/index.html")) {
        Serial.println("index.html not found in LittleFS!");
        request->send(404, "text/plain", "File not found");
        return;
    }
    Serial.println("Sending index.html");
    request->send(LittleFS, "/index.html", "text/html");
}

void CaptivePortal::handlePost(AsyncWebServerRequest *request) {
    Serial.println("=== Handling POST request ===");
    
    String savedSsid = "";
    
    int params = request->params();
    for(int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()) {
            if (p->name() == PARAM_INPUT_1) {
                ssid = p->value().c_str();
                savedSsid = ssid;
                Serial.print("SSID set to: ");
                Serial.println(ssid);
            }
            if (p->name() == PARAM_INPUT_2) {
                pass = p->value().c_str();
                Serial.print("Password set to: ");
                Serial.println(pass);
            }
        }
    }
    
    // 설정 저장
    saveSettings();
    
    // 성공 페이지로 리다이렉트
    String redirectUrl = "/success.html?ssid=" + savedSsid;
    request->redirect(redirectUrl);
    
    Serial.println("Settings saved, redirecting to success page");
    
    // 재부팅 플래그 설정
    shouldReboot = true;
    rebootTime = millis();
    Serial.println("Reboot scheduled in 8 seconds");
}

void CaptivePortal::handleDeviceId(AsyncWebServerRequest *request) {
    Serial.println("Received request for device-id");
    String deviceId = getDeviceId();
    request->send(200, "text/plain", deviceId);
}

void CaptivePortal::handleStaticFiles(AsyncWebServerRequest *request) {
    String path = request->url();
    Serial.print("Received request for: ");
    Serial.println(path);
    
    if (!LittleFS.exists(path)) {
        Serial.print(path);
        Serial.println(" not found in LittleFS!");
        request->send(404, "text/plain", "File not found");
        return;
    }
    
    Serial.print("Sending ");
    Serial.println(path);
    
    if (path.endsWith(".css")) {
        request->send(LittleFS, path, "text/css");
    } else if (path.endsWith(".html")) {
        request->send(LittleFS, path, "text/html");
    } else {
        request->send(LittleFS, path);
    }
}

// WiFi 연결 실패 처리 함수들
void CaptivePortal::onWiFiFailure() {
    wifiFailureCount++;
    Serial.print("WiFi connection failed. Failure count: ");
    Serial.println(wifiFailureCount);
    
    // 실패 카운터를 파일에 저장
    saveFailureCount();
    
    if (wifiFailureCount >= MAX_WIFI_FAILURES) {
        Serial.println("Maximum WiFi failures reached. Will start AP mode on next boot.");
    }
}

void CaptivePortal::resetWiFiFailureCount() {
    wifiFailureCount = 0;
    Serial.println("WiFi failure count reset to 0");
    // 실패 카운터 파일 삭제
    if (LittleFS.exists(failureCountPath)) {
        LittleFS.remove(failureCountPath);
        Serial.println("WiFi failure count file deleted");
    }
}

bool CaptivePortal::shouldStartAPMode() {
    return (wifiFailureCount >= MAX_WIFI_FAILURES);
}

void CaptivePortal::loadFailureCount() {
    String countStr = readFile(LittleFS, failureCountPath);
    if (countStr.length() > 0) {
        wifiFailureCount = countStr.toInt();
        Serial.print("Loaded WiFi failure count: ");
        Serial.println(wifiFailureCount);
    } else {
        wifiFailureCount = 0;
        Serial.println("No WiFi failure count found, starting fresh");
    }
}

void CaptivePortal::saveFailureCount() {
    String countStr = String(wifiFailureCount);
    writeFile(LittleFS, failureCountPath, countStr.c_str());
    Serial.print("Saved WiFi failure count: ");
    Serial.println(wifiFailureCount);
}

// 파일 시스템 함수들 구현
String CaptivePortal::readFile(fs::FS &fs, const char * path) {
    Serial.printf("Reading file: %s\r\n", path);
    
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("- failed to open file for reading");
        return String();
    }
    
    String fileContent;
    while (file.available()) {
        fileContent += String((char)file.read());
    }
    file.close();
    
    Serial.println("- read from file: " + fileContent);
    return fileContent;
}

void CaptivePortal::writeFile(fs::FS &fs, const char * path, const char * message) {
    Serial.printf("Writing file: %s\r\n", path);
    
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("- failed to open file for writing");
        return;
    }
    
    if (file.print(message)) {
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

