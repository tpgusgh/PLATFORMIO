#include "firebase.h"
#include <esp_task_wdt.h>
#include "util.h"

// Firebase 설정 상수 정의 (실제 프로젝트에 맞게 수정 필요)
const char FIREBASE_HOST[] = "otacoesp32iot-d4d62-default-rtdb.asia-southeast1.firebasedatabase.app";
const String FIREBASE_AUTH = "";
const String FIREBASE_PATH = "/esp32devices";
const int SSL_PORT = 443;

// 전역 변수 정의
WiFiClientSecure wifi_client;
HttpClient http_client(wifi_client, FIREBASE_HOST, SSL_PORT);
DeviceInfo deviceInfo;

// 교환기 관련 변수 (main.cpp에서 이동)
uint16_t countA = 0;
uint16_t countB = 0;
uint16_t pulseA = 0;
uint16_t pulseB = 0;

// createResetJsonData 함수 구현 (DeviceInfo 구조체 사용)
String createResetJsonData(const DeviceInfo &info)
{
    // 현재 시간 가져오기
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        DEBUG_PRINTLN("Failed to obtain time");
    }

    // 시간을 원하는 형식으로 변환
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    String currentTime = String(timeStringBuff);

    // Update DeviceInfo structure with current pulse values
    DeviceInfo updatedInfo = info;
    updatedInfo.access_time = currentTime;
    updatedInfo.call = "done";
    updatedInfo.countA = countA;
    updatedInfo.countB = countB;
    updatedInfo.pulseA = pulseA;
    updatedInfo.pulseB = pulseB;
    updatedInfo.remote = 0;

    String updateData = "{";
    updateData += "\"access_time\":\"" + updatedInfo.access_time + "\",";
    updateData += "\"call\":\"" + updatedInfo.call + "\",";
    updateData += "\"countA\":" + String(updatedInfo.countA) + ",";
    updateData += "\"countB\":" + String(updatedInfo.countB) + ",";
    updateData += "\"pulseA\":" + String(updatedInfo.pulseA) + ",";
    updateData += "\"pulseB\":" + String(updatedInfo.pulseB) + ",";
    updateData += "\"remote\":" + String(updatedInfo.remote);
    updateData += "}";

    DEBUG_PRINT("Created update data: ");
    DEBUG_PRINTLN(updateData);

    return updateData;
}

// firebase.h에 선언된 함수들의 기본 구조 구현 (필요시 실제 구현 추가)
bool initFirebase()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTLN("WiFi is not connected.");
        return false;
    }

    // 메모리 상태 확인
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 40000) // 40KB 미만이면 실패
    {
        DEBUG_PRINT("Not enough memory for Firebase initialization. Free heap: ");
        DEBUG_PRINTLN(freeHeap);
        return false;
    }

    DEBUG_PRINTLN("Initializing Firebase...");

    // 간단한 연결 테스트
    WiFiClientSecure testClient;
    testClient.setInsecure();
    testClient.setTimeout(15000); // 15초 타임아웃

    DEBUG_PRINT("Testing connection to: ");
    DEBUG_PRINTLN(FIREBASE_HOST);

    if (testClient.connect(FIREBASE_HOST, SSL_PORT))
    {
        DEBUG_PRINTLN("Firebase connection test successful");
        testClient.stop();
        return true;
    }
    else
    {
        DEBUG_PRINTLN("Firebase connection test failed");
        testClient.stop();
        return false;
    }
}

// 외부 의존 변수/함수 선언 (필요시 실제 구현 또는 extern 처리)
// extern bool isNetworkAvailable; // 제거
extern const char *thingUniqueSerial;
extern String phone;                                                                      // phone 변수 추가
extern void remotePulse(int remoteValue);                                                 // 실제 구현 필요
extern void saveData(uint16_t countA, uint16_t countB, uint16_t pulseA, uint16_t pulseB); // 실제 구현 필요
extern void blinkLED(int count);                                                          // 실제 구현 필요
extern String getCurrentTimeString();                                                     // 실제 구현 필요
extern bool updateFirebaseData(const String &updateData, String &response);               // 실제 구현 필요

// 로컬에서 필요한 상수들 정의
const int MIN_FREE_HEAP = 20000;
const int HTTP_TIMEOUT = 10000;
extern int remainingBlinks;
extern unsigned long lastKeepAliveTime;
extern bool keepAliveLedState;
extern unsigned long keepAliveStartTime;
extern portMUX_TYPE counterMux;

// Firebase 스트림 초기화 함수
bool initFirebaseStream(const String &deviceId)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTLN("WiFi is not connected.");
        return false;
    }

    // 메모리 상태 확인
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 25000) // 25KB 미만이면 실패
    {
        DEBUG_PRINT("Not enough memory for Firebase stream. Free heap: ");
        DEBUG_PRINTLN(freeHeap);
        return false;
    }

    unsigned long startTime;

    WiFiClientSecure localClient;
    localClient.setInsecure();     // 인증서 검증 비활성화 (개발용)
    localClient.setTimeout(30000); // 30초 타임아웃 설정

    HttpClient localHttp(localClient, FIREBASE_HOST, SSL_PORT);

    vTaskDelay(200 / portTICK_PERIOD_MS); // 200ms로 증가

    String streamUrl = String(FIREBASE_PATH) + "/" + deviceId + "/call.json?auth=" + FIREBASE_AUTH;
    DEBUG_PRINT("Initializing stream with URL: ");
    DEBUG_PRINTLN(streamUrl);

    localHttp.beginRequest();
    localHttp.get(streamUrl);
    localHttp.sendHeader("Accept", "text/event-stream");
    localHttp.sendHeader("Connection", "keep-alive");
    localHttp.sendHeader("Cache-Control", "no-cache");
    localHttp.sendHeader("User-Agent", "ESP32-Firebase-Stream");
    localHttp.endRequest();

    // 스트림 응답 대기 (타임아웃 증가)
    startTime = millis();
    while (!localHttp.available() && (millis() - startTime < HTTP_TIMEOUT))
    {
        vTaskDelay(200 / portTICK_PERIOD_MS); // 200ms로 증가
    }

    if (localHttp.available())
    {
        // 초기 응답 확인
        String initialResponse = localHttp.readStringUntil('\n');
        DEBUG_PRINT("Initial stream response: ");
        DEBUG_PRINTLN(initialResponse);

        if (initialResponse.startsWith("HTTP/1.1 200"))
        {
            DEBUG_PRINTLN("Firebase stream initialized successfully");
            return true;
        }
        else
        {
            DEBUG_PRINTLN("Unexpected initial response");
            return false;
        }
    }
    else
    {
        DEBUG_PRINTLN("Failed to initialize Firebase stream - no response");
        return false;
    }
}

// Firebase 스트림 데이터 처리 함수
void handleFirebaseStream(HttpClient &streamHttp)
{
    // Memory shortage check
    if (esp_get_free_heap_size() < 8000)
    { // Stop processing if less than 8KB
        DEBUG_PRINTLN("Low memory in handleFirebaseStream, skipping");
        return;
    }

    // Stop network operations if LED is blinking
    if (remainingBlinks > 0)
    {
        return;
    }

    // 스트림 데이터 처리 (더 안전한 방식)
    int availableCount = 0;
    const int MAX_PROCESS_COUNT = 5; // 최대 5번까지만 처리

    while (streamHttp.available() && availableCount < MAX_PROCESS_COUNT)
    {
        availableCount++;

        // Recheck memory status
        if (esp_get_free_heap_size() < 6000)
        {
            DEBUG_PRINTLN("Memory getting low, stopping stream processing");
            break;
        }

        String line = streamHttp.readStringUntil('\n');
        line.trim();

        // 디버그: 수신된 라인 출력
        DEBUG_PRINT("Received line: ");
        DEBUG_PRINTLN(line);

        // 빈 라인은 무시
        if (line.length() == 0)
            continue;

        // keep-alive 메시지 처리
        if (line.startsWith("event: keep-alive"))
        {
            DEBUG_PRINTLN("Received keep-alive message");
            lastKeepAliveTime = millis();
            keepAliveLedState = true;
            keepAliveStartTime = millis(); // keep-alive 시작 시간 기록
            continue;
        }

        if (line.startsWith("data: "))
        {
            String jsonData = line.substring(6);
            DEBUG_PRINT("Parsing JSON data: ");
            DEBUG_PRINTLN(jsonData);

            if (jsonData == "null")
            {
                DEBUG_PRINTLN("Received null data");
                continue;
            }

            // JSON 데이터가 유효한지 확인
            if (jsonData.length() < 2 || !jsonData.startsWith("{") || !jsonData.endsWith("}"))
            {
                DEBUG_PRINTLN("Invalid JSON format, skipping");
                continue;
            }

            // JSON 파싱을 위한 버퍼 생성 (더 작은 크기 사용)
            JsonDocument doc; // 512바이트로 제한
            DeserializationError error = deserializeJson(doc, jsonData);

            if (error)
            {
                DEBUG_PRINT("JSON parsing failed: ");
                DEBUG_PRINTLN(error.c_str());
                continue;
            }

            // data 필드가 존재하고 null이 아닌지 확인
            if (!doc["data"].is<String>() || doc["data"].isNull())
            {
                DEBUG_PRINTLN("No 'data' field or data is null in JSON");
                continue;
            }

            String callValue = doc["data"].as<String>();
            DEBUG_PRINT("Parsed call value: ");
            DEBUG_PRINTLN(callValue);

            // callValue에 따른 동작 구현
            if (callValue == "reserve")
            {
                DEBUG_PRINTLN("=== START: Processing reserve call ===");
                DEBUG_PRINTLN("Call status is 'reserve', checking remote value...");

                // remote 값 확인
                WiFiClientSecure localClient;
                localClient.setInsecure();
                HttpClient localHttp(localClient, FIREBASE_HOST, SSL_PORT);
                vTaskDelay(100 / portTICK_PERIOD_MS);

                String checkRemoteUrl = FIREBASE_PATH + "/" + thingUniqueSerial + "/remote.json?auth=" + FIREBASE_AUTH;
                DEBUG_PRINT("Remote check URL: ");
                DEBUG_PRINTLN(checkRemoteUrl);

                localHttp.beginRequest();
                localHttp.get(checkRemoteUrl);
                localHttp.sendHeader("Connection", "close");
                localHttp.endRequest();

                // Wait for response
                unsigned long startTime = millis();
                DEBUG_PRINTLN("Waiting for remote value response...");
                while (!localHttp.available() && (millis() - startTime < HTTP_TIMEOUT))
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                if (localHttp.available())
                {
                    int statusCode = localHttp.responseStatusCode();
                    DEBUG_PRINT("Remote check status code: ");
                    DEBUG_PRINTLN(statusCode);

                    String remoteResponse = localHttp.responseBody();
                    remoteResponse.trim();
                    DEBUG_PRINT("Raw remote response: ");
                    DEBUG_PRINTLN(remoteResponse);

                    // Call remotePulse function if remote value is greater than 0
                    if (remoteResponse != "null" && remoteResponse != "0")
                    {
                        int remoteValue = remoteResponse.toInt();
                        DEBUG_PRINT("Remote value is greater than 0: ");
                        DEBUG_PRINTLN(remoteValue);

                        // Actual pulse output implementation (otacoEsp32IOT method)
                        remotePulse(remoteValue);

                        // Save data after remotePulse (otacoEsp32IOT method)
                        saveData(countA, countB, pulseA, pulseB);

                        // Execute Firebase update (otacoEsp32IOT method)
                        DEBUG_PRINTLN("Starting Firebase update...");

                        // Memory status check
                        int freeHeap = ESP.getFreeHeap();
                        if (freeHeap < 10000)
                        {
                            DEBUG_PRINT("Not enough memory for Firebase update. Free heap: ");
                            DEBUG_PRINTLN(freeHeap);
                            DEBUG_PRINTLN("Skipping Firebase update, but LED blink will continue");
                            blinkLED(remoteValue);
                            return;
                        }

                        // Get current time
                        struct tm timeinfo;
                        if (!getLocalTime(&timeinfo))
                        {
                            DEBUG_PRINTLN("Failed to obtain time");
                        }

                        // Convert time to desired format
                        char timeStringBuff[50];
                        strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
                        String currentTime = String(timeStringBuff);

                        // WiFi 신호 강도 가져오기
                        int rssi = WiFi.RSSI();
                        DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                        DEBUG_PRINT(rssi);
                        DEBUG_PRINTLN(" dBm");

                        String updateUrl = FIREBASE_PATH + "/" + thingUniqueSerial + ".json?auth=" + FIREBASE_AUTH;
                        String updateData = "{\"access_time\":\"" + currentTime + "\",\"call\":\"done\",\"remote\":0,\"countA\":" + String(countA) + ",\"countB\":" + String(countB) + ",\"pulseA\":" + String(pulseA) + ",\"pulseB\":" + String(pulseB) + ",\"rssi\":" + String(rssi) + "}";

                        DEBUG_PRINT("Update URL: ");
                        DEBUG_PRINTLN(updateUrl);
                        DEBUG_PRINT("Update data: ");
                        DEBUG_PRINTLN(updateData);

                        // Clean up existing HTTP client and create new client (otacoEsp32IOT method)
                        localHttp.stop();
                        localClient.stop();
                        vTaskDelay(100 / portTICK_PERIOD_MS);

                        localHttp.beginRequest();
                        localHttp.patch(updateUrl);
                        localHttp.sendHeader("Content-Type", "application/json");
                        localHttp.sendHeader("Content-Length", updateData.length());
                        localHttp.sendHeader("Connection", "close");
                        localHttp.beginBody();
                        localHttp.print(updateData);
                        localHttp.endRequest();

                        // Wait for response
                        unsigned long startTime = millis();
                        while (!localHttp.available() && (millis() - startTime < HTTP_TIMEOUT))
                        {
                            vTaskDelay(100 / portTICK_PERIOD_MS);
                        }

                        if (localHttp.available())
                        {
                            int statusCode = localHttp.responseStatusCode();
                            String response = localHttp.responseBody();

                            DEBUG_PRINT("Update status code: ");
                            DEBUG_PRINTLN(statusCode);
                            DEBUG_PRINT("Update response: ");
                            DEBUG_PRINTLN(response);

                            if (statusCode == 200)
                            {
                                DEBUG_PRINTLN("Successfully updated call status to 'done'");
                                // Start LED blink only after all network operations are completed (otacoEsp32IOT method)
                                DEBUG_PRINTLN("All network operations completed, starting LED blink...");
                                blinkLED(remoteValue);
                            }
                            else
                            {
                                DEBUG_PRINT("Failed to update call status. Status code: ");
                                DEBUG_PRINTLN(statusCode);
                                // Start LED blink even on failure (for user feedback)
                                blinkLED(remoteValue);
                            }
                        }
                        else
                        {
                            DEBUG_PRINTLN("No response received for update, but starting LED blink...");
                            blinkLED(remoteValue);
                        }
                    }
                    else
                    {
                        DEBUG_PRINTLN("Remote value is 0 or null, skipping remotePulse");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("No response for remote value check");
                }

                DEBUG_PRINTLN("=== END: Processing reserve call ===");
            }
            else if (callValue == "total_reset")
            {
                DEBUG_PRINTLN("=== START: Processing total_reset call ===");
                DEBUG_PRINTLN("Call status is 'total_reset', performing total reset operation...");

                // 현재 내부 값들 출력 (리셋 전)
                DEBUG_PRINT("Before reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                // 메모리 상태 확인
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 20000) // 20KB 미만이면 실패
                {
                    DEBUG_PRINT("Not enough memory for total reset. Free heap: ");
                    DEBUG_PRINTLN(freeHeap);
                    return;
                }

                // 모든 변수를 0으로 리셋 (임계구역 보호)
                noInterrupts();
                countA = 0;
                countB = 0;
                pulseA = 0;
                pulseB = 0;
                interrupts();

                // 로컬 저장소에 저장
                saveData(countA, countB, pulseA, pulseB);

                // Firebase 업데이트 (리셋된 값들로)
                if (WiFi.status() == WL_CONNECTED)
                {
                    // WiFi 신호 강도 가져오기
                    int rssi = WiFi.RSSI();
                    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                    DEBUG_PRINT(rssi);
                    DEBUG_PRINTLN(" dBm");

                    String updateData = "{\"access_time\":\"" + getCurrentTimeString() + "\",\"call\":\"done\",\"countA\":0,\"countB\":0,\"pulseA\":0,\"pulseB\":0,\"remote\":0,\"rssi\":" + String(rssi) + "}";
                    String response;
                    if (updateFirebaseData(updateData, response))
                    {
                        DEBUG_PRINTLN("Total reset Firebase update successful");
                    }
                    else
                    {
                        DEBUG_PRINTLN("Total reset Firebase update failed");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("WiFi not connected, total reset completed locally only");
                }

                DEBUG_PRINT("After total reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                DEBUG_PRINTLN("Total reset operation completed successfully");
                DEBUG_PRINTLN("=== END: Processing total_reset call ===");
            }
            else if (callValue == "countA_reset")
            {
                DEBUG_PRINTLN("=== START: Processing countA_reset call ===");
                DEBUG_PRINTLN("Call status is 'countA_reset', performing countA reset operation...");

                // 현재 countA 값 출력 (리셋 전)
                DEBUG_PRINT("Before countA reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                // 메모리 상태 확인
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 20000) // 20KB 미만이면 실패
                {
                    DEBUG_PRINT("Not enough memory for countA reset. Free heap: ");
                    DEBUG_PRINTLN(freeHeap);
                    return;
                }

                // countA만 0으로 리셋 (임계구역 보호)
                noInterrupts();
                countA = 0;
                interrupts();

                // 로컬 저장소에 저장
                saveData(countA, countB, pulseA, pulseB);

                // Firebase 업데이트 (countA만 0으로 리셋)
                if (WiFi.status() == WL_CONNECTED)
                {
                    // WiFi 신호 강도 가져오기
                    int rssi = WiFi.RSSI();
                    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                    DEBUG_PRINT(rssi);
                    DEBUG_PRINTLN(" dBm");

                    String updateData = "{\"access_time\":\"" + getCurrentTimeString() + "\",\"call\":\"done\",\"countA\":0,\"countB\":" + String(countB) + ",\"pulseA\":" + String(pulseA) + ",\"pulseB\":" + String(pulseB) + ",\"remote\":0,\"rssi\":" + String(rssi) + "}";
                    String response;
                    if (updateFirebaseData(updateData, response))
                    {
                        DEBUG_PRINTLN("CountA reset Firebase update successful");
                    }
                    else
                    {
                        DEBUG_PRINTLN("CountA reset Firebase update failed");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("WiFi not connected, countA reset completed locally only");
                }

                DEBUG_PRINT("After countA reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                DEBUG_PRINTLN("CountA reset operation completed successfully");
                DEBUG_PRINTLN("=== END: Processing countA_reset call ===");
            }
            else if (callValue == "pulseA_reset")
            {
                DEBUG_PRINTLN("=== START: Processing pulseA_reset call ===");
                DEBUG_PRINTLN("Call status is 'pulseA_reset', performing pulseA reset operation...");

                // 현재 pulseA 값 출력 (리셋 전)
                DEBUG_PRINT("Before pulseA reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                // 메모리 상태 확인
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 20000) // 20KB 미만이면 실패
                {
                    DEBUG_PRINT("Not enough memory for pulseA reset. Free heap: ");
                    DEBUG_PRINTLN(freeHeap);
                    return;
                }

                // pulseA만 0으로 리셋 (임계구역 보호)
                noInterrupts();
                pulseA = 0;
                interrupts();

                // 로컬 저장소에 저장
                saveData(countA, countB, pulseA, pulseB);

                // Firebase 업데이트 (pulseA만 0으로 리셋)
                if (WiFi.status() == WL_CONNECTED)
                {
                    // WiFi 신호 강도 가져오기
                    int rssi = WiFi.RSSI();
                    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                    DEBUG_PRINT(rssi);
                    DEBUG_PRINTLN(" dBm");

                    String updateData = "{\"access_time\":\"" + getCurrentTimeString() + "\",\"call\":\"done\",\"countA\":" + String(countA) + ",\"countB\":" + String(countB) + ",\"pulseA\":0,\"pulseB\":" + String(pulseB) + ",\"remote\":0,\"rssi\":" + String(rssi) + "}";
                    String response;
                    if (updateFirebaseData(updateData, response))
                    {
                        DEBUG_PRINTLN("PulseA reset Firebase update successful");
                    }
                    else
                    {
                        DEBUG_PRINTLN("PulseA reset Firebase update failed");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("WiFi not connected, pulseA reset completed locally only");
                }

                DEBUG_PRINT("After pulseA reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                DEBUG_PRINTLN("PulseA reset operation completed successfully");
                DEBUG_PRINTLN("=== END: Processing pulseA_reset call ===");
            }
            else if (callValue == "countB_reset")
            {
                DEBUG_PRINTLN("=== START: Processing countB_reset call ===");
                DEBUG_PRINTLN("Call status is 'countB_reset', performing countB reset operation...");

                // 현재 countB 값 출력 (리셋 전)
                DEBUG_PRINT("Before countB reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                // 메모리 상태 확인
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 20000) // 20KB 미만이면 실패
                {
                    DEBUG_PRINT("Not enough memory for countB reset. Free heap: ");
                    DEBUG_PRINTLN(freeHeap);
                    return;
                }

                // countB만 0으로 리셋 (임계구역 보호)
                noInterrupts();
                countB = 0;
                interrupts();

                // 로컬 저장소에 저장
                saveData(countA, countB, pulseA, pulseB);

                // Firebase 업데이트 (countB만 0으로 리셋)
                if (WiFi.status() == WL_CONNECTED)
                {
                    // WiFi 신호 강도 가져오기
                    int rssi = WiFi.RSSI();
                    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                    DEBUG_PRINT(rssi);
                    DEBUG_PRINTLN(" dBm");

                    String updateData = "{\"access_time\":\"" + getCurrentTimeString() + "\",\"call\":\"done\",\"countA\":" + String(countA) + ",\"countB\":0,\"pulseA\":" + String(pulseA) + ",\"pulseB\":" + String(pulseB) + ",\"remote\":0,\"rssi\":" + String(rssi) + "}";
                    String response;
                    if (updateFirebaseData(updateData, response))
                    {
                        DEBUG_PRINTLN("CountB reset Firebase update successful");
                    }
                    else
                    {
                        DEBUG_PRINTLN("CountB reset Firebase update failed");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("WiFi not connected, countB reset completed locally only");
                }

                DEBUG_PRINT("After countB reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                DEBUG_PRINTLN("CountB reset operation completed successfully");
                DEBUG_PRINTLN("=== END: Processing countB_reset call ===");
            }
            else if (callValue == "pulseB_reset")
            {
                DEBUG_PRINTLN("=== START: Processing pulseB_reset call ===");
                DEBUG_PRINTLN("Call status is 'pulseB_reset', performing pulseB reset operation...");

                // 현재 pulseB 값 출력 (리셋 전)
                DEBUG_PRINT("Before pulseB reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                // 메모리 상태 확인
                int freeHeap = ESP.getFreeHeap();
                if (freeHeap < 20000) // 20KB 미만이면 실패
                {
                    DEBUG_PRINT("Not enough memory for pulseB reset. Free heap: ");
                    DEBUG_PRINTLN(freeHeap);
                    return;
                }

                // pulseB만 0으로 리셋 (임계구역 보호)
                noInterrupts();
                pulseB = 0;
                interrupts();

                // 로컬 저장소에 저장
                saveData(countA, countB, pulseA, pulseB);

                // Firebase 업데이트 (pulseB만 0으로 리셋)
                if (WiFi.status() == WL_CONNECTED)
                {
                    // WiFi 신호 강도 가져오기
                    int rssi = WiFi.RSSI();
                    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
                    DEBUG_PRINT(rssi);
                    DEBUG_PRINTLN(" dBm");

                    String updateData = "{\"access_time\":\"" + getCurrentTimeString() + "\",\"call\":\"done\",\"countA\":" + String(countA) + ",\"countB\":" + String(countB) + ",\"pulseA\":" + String(pulseA) + ",\"pulseB\":0,\"remote\":0,\"rssi\":" + String(rssi) + "}";
                    String response;
                    if (updateFirebaseData(updateData, response))
                    {
                        DEBUG_PRINTLN("PulseB reset Firebase update successful");
                    }
                    else
                    {
                        DEBUG_PRINTLN("PulseB reset Firebase update failed");
                    }
                }
                else
                {
                    DEBUG_PRINTLN("WiFi not connected, pulseB reset completed locally only");
                }

                DEBUG_PRINT("After pulseB reset - countA: ");
                DEBUG_PRINT(countA);
                DEBUG_PRINT(", countB: ");
                DEBUG_PRINT(countB);
                DEBUG_PRINT(", pulseA: ");
                DEBUG_PRINT(pulseA);
                DEBUG_PRINT(", pulseB: ");
                DEBUG_PRINTLN(pulseB);

                DEBUG_PRINTLN("PulseB reset operation completed successfully");
                DEBUG_PRINTLN("=== END: Processing pulseB_reset call ===");
            }
            else if (callValue == "check")
            {
                DEBUG_PRINTLN("=== Processing check call ===");

                // Firebase Realtime Database 업데이트
                String updateData = "{\"call\":\"done\",\"status\":\"online\"}";
                String response;

                if (updateFirebaseData(updateData, response))
                {
                    DEBUG_PRINTLN("Firebase RDB updated successfully - call: done, status: online");
                }
                else
                {
                    DEBUG_PRINTLN("Failed to update Firebase RDB for check call");
                }

                DEBUG_PRINTLN("=== END: Processing check call ===");
            }
            else if (callValue == "done")
            {
                DEBUG_PRINTLN("=== Processing done call ===");
                // done 상태는 정상적인 완료 상태이므로 특별한 처리 불필요
                DEBUG_PRINTLN("Call completed successfully");
            }
            else
            {
                DEBUG_PRINT("Unknown call value: ");
                DEBUG_PRINTLN(callValue);
            }
        }

        // 각 처리 후 짧은 대기
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

bool updateFirebase(const String &deviceId, bool reconnectStream)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTLN("WiFi is not connected, skipping updateFirebase.");
        return false;
    }

    // 메모리 상태 확인
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 20000) // 20KB 미만이면 실패
    {
        DEBUG_PRINT("Not enough memory for Firebase update. Free heap: ");
        DEBUG_PRINTLN(freeHeap);
        return false;
    }

    // 현재 시간 가져오기
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        DEBUG_PRINTLN("Failed to obtain time");
        return false;
    }

    // 시간을 원하는 형식으로 변환
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    String currentTime = String(timeStringBuff);

    // WiFi 신호 강도 가져오기
    int rssi = WiFi.RSSI();
    DEBUG_PRINT("WiFi Signal Strength (RSSI): ");
    DEBUG_PRINT(rssi);
    DEBUG_PRINTLN(" dBm");

    // 업데이트할 데이터 생성 (RSSI 포함)
    String updateData = "{\"access_time\":\"" + currentTime + "\",\"countA\":" + String(countA) + ",\"countB\":" + String(countB) + ",\"pulseA\":" + String(pulseA) + ",\"pulseB\":" + String(pulseB) + ",\"rssi\":" + String(rssi) + ",\"status\":\"online\"}";

    String response;
    bool result = updateFirebaseData(updateData, response);

    if (result)
    {
        DEBUG_PRINTLN("Firebase update successful");
    }
    else
    {
        DEBUG_PRINTLN("Firebase update failed");
    }

    return result;
}

bool addDeviceIfNotExists(const String &deviceId)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        DEBUG_PRINTLN("WiFi is not connected.");
        return false;
    }

    // 메모리 상태 확인 - 더 보수적으로
    int freeHeap = ESP.getFreeHeap();
    DEBUG_PRINT("Free heap before device check: ");
    DEBUG_PRINTLN(freeHeap);

    if (freeHeap < MIN_FREE_HEAP * 2) // 메모리 요구사항을 2배로 증가
    {
        DEBUG_PRINT("Not enough memory for initial connection. Free heap: ");
        DEBUG_PRINTLN(freeHeap);
        DEBUG_PRINT("Required: ");
        DEBUG_PRINTLN(MIN_FREE_HEAP * 2);
        return false;
    }

    WiFiClientSecure localClient;
    localClient.setInsecure();
    HttpClient localHttp(localClient, FIREBASE_HOST, SSL_PORT);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // 먼저 장치가 존재하는지 확인
    String checkUrl = FIREBASE_PATH + "/" + deviceId + ".json?auth=" + FIREBASE_AUTH;
    DEBUG_PRINT("Checking if device exists at URL: ");
    DEBUG_PRINTLN(checkUrl);

    int retryCount = 0;
    const int maxRetries = 3;
    bool deviceExists = false;
    String response = "";

    while (retryCount < maxRetries && !deviceExists)
    {
        localHttp.beginRequest();
        localHttp.get(checkUrl);
        localHttp.sendHeader("Connection", "close");
        localHttp.endRequest();

        // 응답 대기
        unsigned long timeoutStart = millis();
        while (!localHttp.available() && (millis() - timeoutStart < HTTP_TIMEOUT))
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        int statusCode = localHttp.responseStatusCode();
        response = localHttp.responseBody();

        DEBUG_PRINT("Check device status code: ");
        DEBUG_PRINTLN(statusCode);
        DEBUG_PRINT("Check device response: ");
        DEBUG_PRINTLN(response);

        if (statusCode == 200)
        {
            DEBUG_PRINT("Raw response: '");
            DEBUG_PRINT(response);
            DEBUG_PRINTLN("'");
            DEBUG_PRINT("Response length: ");
            DEBUG_PRINTLN(response.length());

            if (response != "null" && response.length() > 0)
            {
                DEBUG_PRINTLN("Device exists, parsing device info...");
                deviceExists = true;
                break;
            }
            else
            {
                DEBUG_PRINTLN("Device does not exist (null or empty response)");
                deviceExists = false;
                break;
            }
        }
        else if (statusCode == -3)
        {
            DEBUG_PRINTLN("Network error, retrying...");
            retryCount++;
            if (retryCount < maxRetries)
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS); // 1초 대기 후 재시도
                continue;
            }
        }
    }

    // 장치가 이미 존재하면 초기화하지 않음
    if (deviceExists)
    {
        DEBUG_PRINTLN("Device already exists, loading device info...");

        // JSON 파싱을 위한 버퍼 생성 (더 작은 크기 사용)
        JsonDocument doc; // 512바이트로 제한
        DeserializationError error = deserializeJson(doc, response);

        if (!error && response.length() > 0)
        {
            // deviceInfo 구조체 업데이트
            deviceInfo.deviceId = deviceId;
            deviceInfo.access_time = doc["access_time"] | "";
            deviceInfo.address = doc["address"] | "";
            deviceInfo.countA = doc["countA"] | 0;
            deviceInfo.countB = doc["countB"] | 0;
            deviceInfo.pulseA = doc["pulseA"] | 0;
            deviceInfo.pulseB = doc["pulseB"] | 0;
            deviceInfo.name = doc["name"] | "";
            deviceInfo.phone = doc["phone"] | "";
            deviceInfo.status = doc["status"] | "disconnected";
            deviceInfo.remote = doc["remote"] | 0;

            // 전역 변수도 Firebase 값으로 동기화
            DEBUG_PRINT("Before sync - countA: ");
            DEBUG_PRINT(countA);
            DEBUG_PRINT(", countB: ");
            DEBUG_PRINT(countB);
            DEBUG_PRINT(", pulseA: ");
            DEBUG_PRINT(pulseA);
            DEBUG_PRINT(", pulseB: ");
            DEBUG_PRINTLN(pulseB);

            // countA = deviceInfo.countA;
            // countB = deviceInfo.countB;
            // pulseA = deviceInfo.pulseA;
            // pulseB = deviceInfo.pulseB;

            // 동기화된 값을 내부 저장소에도 저장
            // saveData(countA, countB, pulseA, pulseB);

            DEBUG_PRINT("After sync - countA: ");
            DEBUG_PRINT(countA);
            DEBUG_PRINT(", countB: ");
            DEBUG_PRINT(countB);
            DEBUG_PRINT(", pulseA: ");
            DEBUG_PRINT(pulseA);
            DEBUG_PRINT(", pulseB: ");
            DEBUG_PRINTLN(pulseB);

            DEBUG_PRINTLN("Device info loaded successfully");
            DEBUG_PRINT("Name: ");
            DEBUG_PRINTLN(deviceInfo.name);
            DEBUG_PRINT("Address: ");
            DEBUG_PRINTLN(deviceInfo.address);
        }
        else
        {
            DEBUG_PRINT("Failed to parse device info: ");
            DEBUG_PRINTLN(error.c_str());
            DEBUG_PRINT("Response: ");
            DEBUG_PRINTLN(response);
        }
        return true;
    }

    DEBUG_PRINTLN("Device does not exist, creating new device...");

    // 현재 시간 가져오기
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        DEBUG_PRINTLN("Failed to obtain time");
    }

    // 시간을 원하는 형식으로 변환
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    String currentTime = String(timeStringBuff);

    // 장치가 존재하지 않는 경우에만 초기화
    String url = FIREBASE_PATH + "/" + deviceId + ".json?auth=" + FIREBASE_AUTH;
    String jsonData = "{";
    jsonData += "\"access_time\":\"" + currentTime + "\",";
    jsonData += "\"address\":\"\",";
    jsonData += "\"call\":\"done\",";
    jsonData += "\"countA\":0,";
    jsonData += "\"countB\":0,";
    jsonData += "\"name\":\"\",";
    jsonData += "\"phone\":\"" + phone + "\",";
    jsonData += "\"pulseA\":0,";
    jsonData += "\"pulseB\":0,";
    jsonData += "\"remote\":0,";
    jsonData += "\"rssi\":0,";
    jsonData += "\"status\":\"disconnected\"";
    jsonData += "}";

    DEBUG_PRINT("Creating new device at URL: ");
    DEBUG_PRINTLN(url);
    DEBUG_PRINT("Device data: ");
    DEBUG_PRINTLN(jsonData);
    DEBUG_PRINT("Free Heap before request: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());

    // PUT 요청을 위한 별도의 HttpClient 인스턴스 생성
    WiFiClientSecure putClient;
    putClient.setInsecure();
    HttpClient putHttp(putClient, FIREBASE_HOST, SSL_PORT);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    putHttp.beginRequest();
    putHttp.put(url);
    putHttp.sendHeader("Content-Type", "application/json");
    putHttp.sendHeader("Content-Length", jsonData.length());
    putHttp.sendHeader("Connection", "close");
    putHttp.beginBody();
    putHttp.print(jsonData);
    putHttp.endRequest();

    DEBUG_PRINTLN("PUT request sent, waiting for response...");

    // 초기 연결을 위한 더 긴 타임아웃으로 응답 대기
    unsigned long timeoutStart = millis();
    while (!putHttp.available() && (millis() - timeoutStart < HTTP_TIMEOUT * 2))
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if ((millis() - timeoutStart) % 1000 == 0)
        {
            DEBUG_PRINT("Waiting for response... ");
            DEBUG_PRINT(millis() - timeoutStart);
            DEBUG_PRINTLN("ms elapsed");
        }
    }

    DEBUG_PRINTLN("Response received or timeout reached");

    int putStatus = putHttp.responseStatusCode();
    String putResponse = putHttp.responseBody();

    DEBUG_PRINT("Add Device PUT status: ");
    DEBUG_PRINTLN(putStatus);
    DEBUG_PRINT("PUT Response: ");
    DEBUG_PRINTLN(putResponse);
    DEBUG_PRINT("Free Heap after request: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());

    return putStatus == 200;
}

bool updateFirebaseData(const String &updateData, String &response)
{
    // 메모리 상태 확인
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 20000) // 20KB 미만이면 실패
    {
        DEBUG_PRINT("Not enough memory for Firebase update. Free heap: ");
        DEBUG_PRINTLN(freeHeap);
        return false;
    }

    WiFiClientSecure localClient;
    localClient.setInsecure();
    localClient.setTimeout(15000); // 15초로 단축 (더 빠른 실패)
    HttpClient localHttp(localClient, FIREBASE_HOST, SSL_PORT);
    vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms로 단축

    String updateUrl = FIREBASE_PATH + "/" + thingUniqueSerial + ".json?auth=" + FIREBASE_AUTH;

    DEBUG_PRINT("Update URL: ");
    DEBUG_PRINTLN(updateUrl);
    DEBUG_PRINT("Update data: ");
    DEBUG_PRINTLN(updateData);

    localHttp.beginRequest();
    localHttp.patch(updateUrl);
    localHttp.sendHeader("Content-Type", "application/json");
    localHttp.sendHeader("Content-Length", updateData.length());
    localHttp.sendHeader("Connection", "close");
    localHttp.beginBody();
    localHttp.print(updateData);
    localHttp.endRequest();

    // 응답 대기 (타임아웃 단축)
    unsigned long startTime = millis();
    while (!localHttp.available() && (millis() - startTime < HTTP_TIMEOUT))
    {
        vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms로 단축
    }

    if (localHttp.available())
    {
        int statusCode = localHttp.responseStatusCode();
        response = localHttp.responseBody();

        DEBUG_PRINT("Update status code: ");
        DEBUG_PRINTLN(statusCode);
        DEBUG_PRINT("Update response: ");
        DEBUG_PRINTLN(response);

        return statusCode == 200;
    }

    DEBUG_PRINTLN("No response received for update");
    return false;
}

bool recoverMemory()
{
    // TODO: Memory recovery implementation
    return true;
}
