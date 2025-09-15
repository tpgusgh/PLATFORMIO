#include "myFileSystem.h"

// LittleFS 초기화 함수
bool initLittleFS()
{
  // LittleFS 마운트 (실패 시 포맷 시도)
  if (!LittleFS.begin(true))
  {
    Serial.println("[LittleFS] mount failed");
    return false;
  }
  
  Serial.println("[LittleFS] mount successful");
  return true;
}



// 파일시스템 정보 출력 함수
void printFilesystemInfo()
{
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  Serial.print("[LittleFS] total=");
  Serial.print(totalBytes);
  Serial.print(" bytes, used=");
  Serial.print(usedBytes);
  Serial.println(" bytes");
}



bool readLittleFS()
{
  File f = LittleFS.open("/myWiFi.txt", FILE_READ);

  while(f.available())
  {
    Serial.write(f.read());
  }
  f.close();
}


// 트리 형태로 구조를 예쁘게 출력
void printTree(fs::FS &fs, const char *dirname, uint8_t levels, uint8_t indent)
{
  File root = fs.open(dirname);
  if (!root || !root.isDirectory())
  {
    Serial.println("[LittleFS] tree print failed: invalid path");
    return;
  }

  File entry = root.openNextFile();
  while (entry)
  {
    for (uint8_t i = 0; i < indent; i++)
      Serial.print("  ");
    if (entry.isDirectory())
    {
      Serial.print("|- ");
      Serial.print(entry.name());
      Serial.println("/");
      if (levels)
      {
        printTree(fs, entry.name(), levels - 1, indent + 1);
      }
    }
    else
    {
      Serial.print("|- ");
      Serial.print(entry.name());
      Serial.print("  (");
      Serial.print(entry.size());
      Serial.println(" bytes)");
    }
    entry = root.openNextFile();
  }
}

// 기본 파일 쓰기/읽기 테스트 함수
void testBasicFileOperations()
{
  // 기본 쓰기 테스트
  File f = LittleFS.open("/hello.txt", FILE_WRITE);
  f.println("Hello LittleFS");
  f.close();

  // 읽기 테스트
  f = LittleFS.open("/hello.txt", FILE_READ);

  Serial.println("[LittleFS] /hello.txt content:");
  while (f.available())
  {
    Serial.write(f.read());
  }
  f.close();
}

// 파일시스템 전체 테스트 함수
void runFilesystemTests()
{
  Serial.println("[LittleFS] Starting filesystem tests...");
  
  // 파일시스템 정보 출력
  printFilesystemInfo();
  
  // 기본 파일 작업 테스트
  testBasicFileOperations();
  
  // 트리 구조 출력
  Serial.println("[LittleFS] tree:");
  printTree(LittleFS, "/", 3, 0);
  
  Serial.println("[LittleFS] Filesystem tests completed");
}

// WiFi 설정 저장 함수
bool saveWiFiConfig(const char* ssid, const char* password)
{
  if (ssid == nullptr || password == nullptr) {
    Serial.println("[WiFi Config] Error: SSID or password is null");
    return false;
  }
  
  File file = LittleFS.open("/myWiFi.txt", FILE_WRITE);
  if (!file) {
    Serial.println("[WiFi Config] Error: Failed to open file for writing");
    return false;
  }
  
  // JSON 형태로 저장
  file.print("{\"ssid\":\"");
  file.print(ssid);
  file.print("\",\"password\":\"");
  file.print(password);
  file.print("\"}");
  file.close();
  
  Serial.println("[WiFi Config] WiFi configuration saved successfully");
  return true;
}

// WiFi 설정 읽기 함수
bool loadWiFiConfig(char* ssid, char* password, size_t maxLength)
{
  if (ssid == nullptr || password == nullptr || maxLength == 0) {
    Serial.println("[WiFi Config] Error: Invalid parameters");
    return false;
  }
  
  File file = LittleFS.open("/myWiFi.txt", FILE_READ);
  if (!file) {
    Serial.println("[WiFi Config] Error: Failed to open file for reading");
    return false;
  }
  
  // 파일 내용 읽기
  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  
  // JSON 파싱 (간단한 방식)
  int ssidStart = content.indexOf("\"ssid\":\"") + 8;
  int ssidEnd = content.indexOf("\"", ssidStart);
  int passwordStart = content.indexOf("\"password\":\"") + 12;
  int passwordEnd = content.indexOf("\"", passwordStart);
  
  if (ssidStart == -1 || ssidEnd == -1 || passwordStart == -1 || passwordEnd == -1) {
    Serial.println("[WiFi Config] Error: Invalid file format");
    return false;
  }
  
  // SSID 추출
  String ssidStr = content.substring(ssidStart, ssidEnd);
  if (ssidStr.length() >= maxLength) {
    Serial.println("[WiFi Config] Error: SSID too long");
    return false;
  }
  strcpy(ssid, ssidStr.c_str());
  
  // Password 추출
  String passwordStr = content.substring(passwordStart, passwordEnd);
  if (passwordStr.length() >= maxLength) {
    Serial.println("[WiFi Config] Error: Password too long");
    return false;
  }
  strcpy(password, passwordStr.c_str());
  
  Serial.println("[WiFi Config] WiFi configuration loaded successfully");
  Serial.printf("[WiFi Config] SSID: %s\n", ssid);
  return true;
}
