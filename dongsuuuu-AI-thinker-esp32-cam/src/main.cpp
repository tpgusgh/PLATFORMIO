#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>
#include "util.h"
#include <Preferences.h>
#include "LittleFS.h"
#include <ArduinoJson.h>
#include <time.h>
#include "firebase.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_system.h>
#include <esp_heap_caps.h>

#pragma region 전역 변수 선언
#define LED_WHITE_BUILTIN (4) // ESP32-CAM built-in LED pin
#define LED_STATUS (33)       // LED1
#define LED_INFO (32)         // LED2
#define BTN_USER (14)         // 47k 풀업되있음
#define PULSE_OUT (25)

// 시스템 관련 변수
const char *thingUniqueSerial;

// 와이파이 관련 변수
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *phonePath = "/phone.txt";
// String ssid = "otaco";
// String pass = "!@red0221";
String ssid = "";
String pass = "";
String phone = "";
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "phone";

// 교환기 관련 변수 (firebase.cpp에서 정의됨)
extern uint16_t countA, countB, pulseA, pulseB;

// 지폐 인식 관련 변수 추가 (otacoEsp32IOT 방식)
uint32_t tempAmount = 0;
unsigned long lastUpdateTime = 0;

// RTOS 및 시리얼 관련 전역 변수 및 선언
TaskHandle_t SerialCommuteTaskHandle = NULL;
TaskHandle_t Serial2CommuteTaskHandle = NULL;
QueueHandle_t serialQueue;
QueueHandle_t serial2Queue;
// 인터럽트 기반 데이터 버퍼 및 플래그
#define BUFFER_SIZE 128
volatile char serialBuffer[BUFFER_SIZE];
volatile char serial2Buffer[BUFFER_SIZE];
volatile int serialBufferIndex = 0;
volatile int serial2BufferIndex = 0;
volatile bool serialTimeoutFlag = false;
volatile bool serial2TimeoutFlag = false;
volatile unsigned long lastSerialCharTime = 0;
volatile unsigned long lastSerial2CharTime = 0;

// LED 관련 전역 변수들
unsigned long previousLedMillis = 0;          // LED 마지막 상태 변경 시간
const long ledInterval = 1000;                // LED 깜빡임 간격 (1초)
bool ledBlinkState = false;                   // LED 현재 상태
unsigned long lastLedBlinkTime = 0;           // LED 점멸 마지막 시간
const unsigned long LED_BLINK_INTERVAL = 250; // 0.25초
int remainingBlinks = 0;                      // 남은 점멸 횟수
unsigned long ledStartTime = 0;               // LED 점멸 시작 시간
int currentRemoteValue = 0;                   // 현재 remote 값 저장

// LED 상태 표시 관련 전역 변수 및 enum
enum SystemStatus
{
  STATUS_BOOTING,      // 부팅 중 (LED 켜짐)
  STATUS_WIFI_CONNECTING, // WiFi 연결 중 (LED 켜짐)
  STATUS_FIREBASE_CONNECTING, // Firebase 연결 중 (LED 켜짐)
  STATUS_CONNECTED,    // 모든 연결 완료 (LED 꺼짐)
  STATUS_AP_MODE,      // AP 모드 (LED 깜빡임)
  STATUS_OFFLINE       // 연결 실패 (LED 꺼짐)
};
volatile SystemStatus currentSystemStatus = STATUS_BOOTING;
unsigned long lastStatusLedTime = 0;
bool statusLedState = false;
// ====== Firebase 스트림 관련 외부 변수 정의 (빌드 에러 방지) ======
bool isNetworkAvailable = true; // 네트워크 사용 가능 여부 (실제 네트워크 체크 로직 필요시 수정)
int HTTP_TIMEOUT = 10000;       // 10초 타임아웃 (ms)
unsigned long lastKeepAliveTime = 0;
bool keepAliveLedState = false;
unsigned long keepAliveStartTime = 0;
const unsigned long KEEP_ALIVE_DURATION = 25000; // 25초로 변경
const unsigned long KEEP_ALIVE_ON_TIME = 100;    // 100ms 켜짐
const unsigned long KEEP_ALIVE_OFF_TIME = 500;   // 500ms 꺼짐
// Firebase 스트림 태스크 핸들 (재활성화)
TaskHandle_t FirebaseStreamTaskHandle = NULL;

// AP 모드 관련 전역 변수 추가
AsyncWebServer server(80);
bool isAPMode = false;

// 재부팅 지연을 위한 전역 변수 추가
bool shouldReboot = false;
unsigned long rebootTime = 0;
const unsigned long REBOOT_DELAY = 8000; // 8초 후 재부팅

// 버튼 관련 전역 변수 추가
unsigned long lastDebounceTime = 0;     // 마지막 디바운스 시간
const unsigned long debounceDelay = 50; // 디바운스 지연 시간 (50ms)
int lastButtonState = HIGH;             // 버튼의 마지막 상태
int buttonState = HIGH;                 // 버튼의 현재 상태
unsigned long buttonPressStartTime = 0; // 버튼을 누른 시작 시간
bool isLongPress = false;               // 길게 누르기 상태

// Firebase 스트림 태스크 함수 (비동기 상태머신 방식 - otacoEsp32IOT 안정성 적용)
enum StreamConnectState
{
  STREAM_IDLE,
  STREAM_CONNECTING,
  STREAM_CONNECTED,
  STREAM_FAILED
};
StreamConnectState streamState = STREAM_IDLE;
unsigned long streamConnectStart = 0;
const unsigned long STREAM_CONNECT_TIMEOUT = 20000;    // 20초로 증가
const unsigned long CONNECTION_RETRY_INTERVAL = 15000; // 15초로 증가
const unsigned long STREAM_TIMEOUT = 90000;            // 90초로 증가
unsigned long lastConnectionAttempt = 0;

// ====== 비즈니스 로직 함수들 (BusinessLogicTask에서 호출됨) ======
void processSerialBusinessLogic(const String &receivedData)
{
  // 지폐 인식 시 카운트 변수 업데이트 로직 추가
  // otacoEsp32IOT의 sBle 처리 로직 참조

  // "sBle" 수신 시 금액 누적 (otacoEsp32IOT 방식)
  if (receivedData == "sBle")
  {
    DEBUG_PRINT("Received sBle command, tempAmount: ");
    DEBUG_PRINTLN(tempAmount);

    // 메모리 부족 시 처리 건너뛰기
    if (ESP.getFreeHeap() < 30000)
    {
      DEBUG_PRINT("Insufficient memory for sBle processing. Free heap: ");
      DEBUG_PRINTLN(ESP.getFreeHeap());
      return;
    }

    // 임계구역 보호 - 변수 업데이트만
    noInterrupts();
    countA += tempAmount;
    countB = countA;
    interrupts();

    // saveData는 interrupts 밖에서 실행 (시간이 오래 걸림)
    saveData(countA, countB, pulseA, pulseB);

    DEBUG_PRINT("Total Amount updated: ");
    DEBUG_PRINTLN(countA);

    // Firebase 업데이트는 WiFi 연결 시에만 실행 (5초 디바운스)
    if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateTime > 5000)
    {
      if (ESP.getFreeHeap() > 50000)
      {
        lastUpdateTime = millis();
        DEBUG_PRINTLN("Attempting Firebase update...");
        bool updateResult = updateFirebase(thingUniqueSerial, false);
        if (!updateResult)
        {
          DEBUG_PRINTLN("Firebase update failed, will retry when WiFi reconnects");
        }
      }
      else
      {
        DEBUG_PRINTLN("Insufficient memory for Firebase update");
      }
    }
    // WiFi 연결이 안되어 있으면 RDB 업데이트만 건너뛰고, WiFi 재연결은 WiFiCheckTask에서 계속 시도
  }
}

void processSerial2BusinessLogic(const String &receivedData)
{
  // 지폐 금액 설정 로직 추가 (otacoEsp32IOT 방식)

  // 패킷에 따른 임시 금액 설정
  if (receivedData == "sC1e")
  {
    tempAmount = 1;
    DEBUG_PRINTLN("Set tempAmount to 1");
  }
  else if (receivedData == "sC2e")
  {
    tempAmount = 5;
    DEBUG_PRINTLN("Set tempAmount to 5");
  }
  else if (receivedData == "sC3e")
  {
    tempAmount = 10;
    DEBUG_PRINTLN("Set tempAmount to 10");
  }
  else if (receivedData == "sC4e")
  {
    tempAmount = 50;
    DEBUG_PRINTLN("Set tempAmount to 50");
  }
}

// remotePulse 함수 구현 (otacoEsp32IOT 참고)
void remotePulse(int remoteValue)
{
  DEBUG_PRINTLN("=== START: remotePulse function called ===");
  DEBUG_PRINT("Remote Pulse Value: ");
  DEBUG_PRINTLN(remoteValue);
  DEBUG_PRINT("Previous countA: ");
  DEBUG_PRINTLN(countA);
  DEBUG_PRINT("Previous countB: ");
  DEBUG_PRINTLN(countB);
  DEBUG_PRINT("Previous pulseA: ");
  DEBUG_PRINTLN(pulseA);
  DEBUG_PRINT("Previous pulseB: ");
  DEBUG_PRINTLN(pulseB);

  currentRemoteValue = remoteValue; // Store remote value

  // Memory status check
  int freeHeap = ESP.getFreeHeap();
  if (freeHeap < 10000) // Stop processing if less than 10KB
  {
    DEBUG_PRINT("Not enough memory for remotePulse. Free heap: ");
    DEBUG_PRINTLN(freeHeap);
    DEBUG_PRINTLN("Skipping remotePulse operation");
    return;
  }

  // 임계구역 보호 - 펄스만 업데이트 (reserve 명령은 펄스만 변경)
  noInterrupts();
  pulseA += remoteValue;
  pulseB += remoteValue;
  interrupts();

  DEBUG_PRINT("Updated pulseA: ");
  DEBUG_PRINTLN(pulseA);
  DEBUG_PRINT("Updated pulseB: ");
  DEBUG_PRINTLN(pulseB);

  DEBUG_PRINTLN("=== END: remotePulse function completed ===");
}

// blinkLED 함수 구현 (otacoEsp32IOT 참고)
void blinkLED(int count)
{
  if (count > 0)
  {
    remainingBlinks = count * 2; // Set to 2x for on/off actions
    ledStartTime = millis();     // Record start time
    lastLedBlinkTime = ledStartTime;
    ledBlinkState = false;        // Start with false so LED turns on in first blink
    digitalWrite(LED_INFO, HIGH); // Turn off
    digitalWrite(PULSE_OUT, LOW);

    DEBUG_PRINTLN("=== LED Blink Started ===");
    DEBUG_PRINT("Count: ");
    DEBUG_PRINT(count);
    DEBUG_PRINT(" | Total blinks: ");
    DEBUG_PRINT(remainingBlinks);
    DEBUG_PRINT(" | Interval: ");
    DEBUG_PRINT(LED_BLINK_INTERVAL);
    DEBUG_PRINTLN("ms ===");
  }
}

// LEDBlinkTask 함수 구현 (otacoEsp32IOT 참고)

void LEDBlinkTask(void *parameter)
{
  while (true)
  {
    if (remainingBlinks > 0)
    {
      unsigned long currentMillis = millis();

      // Check if LED blink interval has passed
      if (currentMillis - lastLedBlinkTime >= LED_BLINK_INTERVAL)
      {
        lastLedBlinkTime = currentMillis;
        ledBlinkState = !ledBlinkState;

        // LED 상태 변경
        digitalWrite(LED_INFO, ledBlinkState ? LOW : HIGH);
        digitalWrite(PULSE_OUT, ledBlinkState ? HIGH : LOW);

        // 디버그 출력
        DEBUG_PRINT("LED Blink: ");
        DEBUG_PRINT(ledBlinkState ? "ON" : "OFF");
        DEBUG_PRINT(" | Remaining: ");
        DEBUG_PRINTLN(remainingBlinks);

        remainingBlinks--;

        // Turn off LED and initialize when all blinks are completed
        if (remainingBlinks == 0)
        {
          digitalWrite(LED_INFO, HIGH); // Turn off
          digitalWrite(PULSE_OUT, LOW);
          DEBUG_PRINTLN("LED blink sequence completed!");

          // Set flag to resume network operations when LED blink is completed
          DEBUG_PRINTLN("Network operations can resume now");
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms 딜레이
  }
}

void FirebaseStreamTask(void *parameter)
{
  DEBUG_PRINTLN("FirebaseStreamTask started - Stream Mode");

  static WiFiClientSecure streamClient;
  static HttpClient streamHttp(streamClient, FIREBASE_HOST, SSL_PORT);
  static bool streamInitialized = false;
  static unsigned long lastStreamActivity = 0;
  static unsigned long lastConnectionAttempt = 0;
  const unsigned long STREAM_TIMEOUT = 60000;            // 60초 타임아웃
  const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30초로 증가
  const unsigned long SSL_TIMEOUT = 30000;               // SSL 타임아웃

  while (true)
  {
    // 메모리 상태 확인
    int freeHeap = ESP.getFreeHeap();
    if (freeHeap < 40000) // 40KB 미만이면 대기
    {
      DEBUG_PRINT("Low memory detected: ");
      DEBUG_PRINTLN(freeHeap);
      vTaskDelay(15000 / portTICK_PERIOD_MS); // 15초 대기
      continue;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      // 스트림 연결이 안 되어 있으면 새로 연결
      if (!streamInitialized || !streamHttp.connected() ||
          (millis() - lastStreamActivity > STREAM_TIMEOUT))
      {
        // 연결 시도 간격 확인
        if (millis() - lastConnectionAttempt < CONNECTION_RETRY_INTERVAL)
        {
          vTaskDelay(5000 / portTICK_PERIOD_MS); // 5초 대기
          continue;
        }

        lastConnectionAttempt = millis();
        DEBUG_PRINTLN("=== Initializing/reconnecting Firebase stream ===");
        DEBUG_PRINT("WiFi Status: ");
        DEBUG_PRINTLN(WiFi.status());
        DEBUG_PRINT("WiFi IP: ");
        DEBUG_PRINTLN(WiFi.localIP());
        DEBUG_PRINT("Free Heap: ");
        DEBUG_PRINTLN(ESP.getFreeHeap());

        // SSL 클라이언트 완전 초기화
        streamClient.stop();
        streamHttp.stop();
        vTaskDelay(2000 / portTICK_PERIOD_MS); // 2초로 증가

        // SSL 설정 강화
        streamClient.setInsecure();
        streamClient.setTimeout(SSL_TIMEOUT);

        String streamUrl = FIREBASE_PATH + "/" + thingUniqueSerial + "/call.json?auth=" + FIREBASE_AUTH;
        DEBUG_PRINT("Stream URL: ");
        DEBUG_PRINTLN(streamUrl);

        streamHttp.beginRequest();
        streamHttp.get(streamUrl);
        streamHttp.sendHeader("Accept", "text/event-stream");
        streamHttp.sendHeader("Connection", "keep-alive");
        streamHttp.sendHeader("Cache-Control", "no-cache");
        streamHttp.sendHeader("User-Agent", "ESP32-Firebase-Stream");
        streamHttp.endRequest();

        DEBUG_PRINTLN("Stream request sent, waiting for response...");

        // 초기 응답 대기 (타임아웃 증가)
        unsigned long startTime = millis();
        bool responseReceived = false;

        while (!responseReceived && (millis() - startTime < 25000)) // 25초로 증가
        {
          if (streamHttp.available())
          {
            String initialResponse = streamHttp.readStringUntil('\n');
            DEBUG_PRINT("Stream initial response: ");
            DEBUG_PRINTLN(initialResponse);

            if (initialResponse.startsWith("HTTP/1.1 200"))
            {
              DEBUG_PRINTLN("=== Firebase stream connected successfully ===");
              streamInitialized = true;
              lastStreamActivity = millis();
              currentSystemStatus = STATUS_CONNECTED; // 모든 연결 완료 상태
              responseReceived = true;
            }
            else
            {
              DEBUG_PRINT("Stream connection failed with response: ");
              DEBUG_PRINTLN(initialResponse);
              streamInitialized = false;
              responseReceived = true;
            }
          }
          else
          {
            vTaskDelay(200 / portTICK_PERIOD_MS);

            // 5초마다 상태 출력
            if ((millis() - startTime) % 5000 == 0)
            {
              DEBUG_PRINT("Waiting for stream response... ");
              DEBUG_PRINT(millis() - startTime);
              DEBUG_PRINTLN("ms elapsed");
            }
          }
        }

        if (!responseReceived)
        {
          DEBUG_PRINTLN("No response from stream connection - timeout occurred");
          streamInitialized = false;
          vTaskDelay(10000 / portTICK_PERIOD_MS); // 10초 대기 후 재시도
        }
      }
      else
      {
        // 스트림이 연결되어 있으면 데이터 처리
        if (streamHttp.available())
        {
          lastStreamActivity = millis();
          handleFirebaseStream(streamHttp);
        }
        else
        {
          // 데이터가 없으면 짧게 대기
          vTaskDelay(500 / portTICK_PERIOD_MS);
        }
      }
    }
    else
    {
      DEBUG_PRINTLN("WiFi not connected");
      streamInitialized = false;
      currentSystemStatus = STATUS_OFFLINE; // WiFi 연결 안됨 상태
      vTaskDelay(10000 / portTICK_PERIOD_MS); // 10초 대기
    }
  }
}

#define IO_UART 1
#define BILL_UART 2
#define UART_BUF_SIZE 128

// 큐 핸들 추가
QueueHandle_t ioUartQueue;
QueueHandle_t billUartQueue;

void io_uart_task(void *pvParameters)
{
  uint8_t buffer[4];
  while (true)
  {
    int len = uart_read_bytes(IO_UART, buffer, 4, 100 / portTICK_PERIOD_MS);
    if (len == 4)
    {
      uart_write_bytes(BILL_UART, (const char *)buffer, 4);

      DEBUG_PRINT("BILL <- IO: ");
      for (int i = 0; i < 4; i++)
      {
        DEBUG_PRINT((char)buffer[i]);
      }
      DEBUG_PRINTLN();

      // 큐에 데이터 전송 (비동기 처리)
      if (ioUartQueue != NULL)
      {
        char tempBuffer[5];
        memcpy(tempBuffer, buffer, 4);
        tempBuffer[4] = '\0';
        xQueueSend(ioUartQueue, tempBuffer, 0); // 즉시 전송, 실패해도 무시
      }
    }
  }
}

void bill_uart_task(void *pvParameters)
{
  uint8_t buffer[4];
  while (true)
  {
    int len = uart_read_bytes(BILL_UART, buffer, 4, 100 / portTICK_PERIOD_MS);
    if (len == 4)
    {
      uart_write_bytes(IO_UART, (const char *)buffer, 4);

      DEBUG_PRINT("BILL -> IO: ");
      for (int i = 0; i < 4; i++)
      {
        DEBUG_PRINT((char)buffer[i]);
      }
      DEBUG_PRINTLN();

      // 큐에 데이터 전송 (비동기 처리)
      if (billUartQueue != NULL)
      {
        char tempBuffer[5];
        memcpy(tempBuffer, buffer, 4);
        tempBuffer[4] = '\0';
        xQueueSend(billUartQueue, tempBuffer, 0); // 즉시 전송, 실패해도 무시
      }
    }
  }
}

// 비즈니스 로직 처리 태스크
void BusinessLogicTask(void *pvParameters)
{
  char receivedData[5];

  while (true)
  {
    // IO UART 큐에서 데이터 처리
    if (xQueueReceive(ioUartQueue, receivedData, 10 / portTICK_PERIOD_MS) == pdTRUE)
    {
      String dataStr = String(receivedData);
      processSerialBusinessLogic(dataStr);
    }

    // BILL UART 큐에서 데이터 처리
    if (xQueueReceive(billUartQueue, receivedData, 10 / portTICK_PERIOD_MS) == pdTRUE)
    {
      String dataStr = String(receivedData);
      processSerial2BusinessLogic(dataStr);
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); // 10ms 대기
  }
}

// 시리얼 태스크 함수 선언
void SerialCommuteTask(void *parameter);
void WiFiCheckTask(void *parameter);
void BusinessLogicTask(void *parameter);
void NetworkInitTask(void *parameter); // WiFi 설정을 위한 네트워크 초기화 태스크
void ButtonInputTask(void *parameter); // 버튼 입력 처리 태스크
void RebootTask(void *parameter);      // 지연 재부팅 처리 태스크

// LED 상태 표시 태스크
void LEDStatusTask(void *parameter)
{
  static SystemStatus lastStatus = STATUS_BOOTING;
  while (true)
  {
    unsigned long currentMillis = millis();
    if (currentSystemStatus != lastStatus)
    {
      lastStatus = currentSystemStatus;
      DEBUG_PRINT("System status changed to: ");
      switch (currentSystemStatus)
      {
        case STATUS_BOOTING: DEBUG_PRINTLN("BOOTING"); break;
        case STATUS_WIFI_CONNECTING: DEBUG_PRINTLN("WIFI_CONNECTING"); break;
        case STATUS_FIREBASE_CONNECTING: DEBUG_PRINTLN("FIREBASE_CONNECTING"); break;
        case STATUS_CONNECTED: DEBUG_PRINTLN("CONNECTED"); break;
        case STATUS_AP_MODE: DEBUG_PRINTLN("AP_MODE"); break;
        case STATUS_OFFLINE: DEBUG_PRINTLN("OFFLINE"); break;
      }
    }
    
    switch (currentSystemStatus)
    {
    case STATUS_BOOTING:
      // 부팅 중: LED 켜짐
      if (!statusLedState)
      {
        digitalWrite(LED_STATUS, LOW); // 켜기
        statusLedState = true;
        DEBUG_PRINTLN("LED: ON (Booting)");
      }
      break;
      
    case STATUS_WIFI_CONNECTING:
      // WiFi 연결 중: LED 켜짐
      if (!statusLedState)
      {
        digitalWrite(LED_STATUS, LOW); // 켜기
        statusLedState = true;
        DEBUG_PRINTLN("LED: ON (WiFi connecting)");
      }
      break;
      
    case STATUS_FIREBASE_CONNECTING:
      // Firebase 연결 중: LED 켜짐
      if (!statusLedState)
      {
        digitalWrite(LED_STATUS, LOW); // 켜기
        statusLedState = true;
        DEBUG_PRINTLN("LED: ON (Firebase connecting)");
      }
      break;
      
    case STATUS_CONNECTED:
      // 모든 연결 완료: keep_alive 상태에 따라 LED 제어
      if (keepAliveLedState)
      {
        // keep_alive 수신 중: 100ms 켜짐, 500ms 꺼짐 패턴
        unsigned long elapsedTime = currentMillis - keepAliveStartTime;
        if (elapsedTime < KEEP_ALIVE_DURATION)
        {
          // 25초 동안 keep_alive 패턴 유지
          unsigned long cycleTime = elapsedTime % (KEEP_ALIVE_ON_TIME + KEEP_ALIVE_OFF_TIME);
          bool shouldBeOn = cycleTime < KEEP_ALIVE_ON_TIME;
          
          if (shouldBeOn && !statusLedState)
          {
            digitalWrite(LED_STATUS, LOW); // 켜기
            statusLedState = true;
          }
          else if (!shouldBeOn && statusLedState)
          {
            digitalWrite(LED_STATUS, HIGH); // 끄기
            statusLedState = false;
          }
        }
        else
        {
          // 25초 경과 후 LED 끄기
          keepAliveLedState = false;
          if (statusLedState)
          {
            digitalWrite(LED_STATUS, HIGH); // 끄기
            statusLedState = false;
            DEBUG_PRINTLN("LED: OFF (Keep-alive duration completed)");
          }
        }
      }
      else
      {
        // keep_alive 수신 안됨: LED 꺼짐
        if (statusLedState)
        {
          digitalWrite(LED_STATUS, HIGH); // 끄기
          statusLedState = false;
          DEBUG_PRINTLN("LED: OFF (Connected, no keep-alive)");
        }
      }
      break;
      
    case STATUS_AP_MODE:
      // AP 모드: 빠르게 깜빡임
      if (currentMillis - lastStatusLedTime >= 100)
      {
        lastStatusLedTime = currentMillis;
        statusLedState = !statusLedState;
        digitalWrite(LED_STATUS, statusLedState ? LOW : HIGH); // LOW=켜기, HIGH=끄기
      }
      break;
      
    case STATUS_OFFLINE:
      // 연결 실패: 10ms 켜짐, 2초 꺼짐 무한 반복
      unsigned long offlineCycleTime = currentMillis % 2010; // 10ms + 2000ms = 2010ms 주기
      bool shouldBeOn = offlineCycleTime < 10; // 처음 10ms만 켜짐
      
      if (shouldBeOn && !statusLedState)
      {
        digitalWrite(LED_STATUS, LOW); // 켜기
        statusLedState = true;
      }
      else if (!shouldBeOn && statusLedState)
      {
        digitalWrite(LED_STATUS, HIGH); // 끄기
        statusLedState = false;
      }
      break;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// WiFi 상태 체크 및 자동 재연결 태스크
// 논블로킹 상태머신 방식으로 변경
enum WiFiReconnectState
{
  WIFI_IDLE,
  WIFI_RECONNECTING,
  WIFI_WAIT_BEFORE_RETRY
};
WiFiReconnectState wifiReconnectState = WIFI_IDLE;
unsigned long reconnectStartTime = 0;
unsigned long waitBeforeRetryStart = 0;
const unsigned long RECONNECT_TIMEOUT = 20000;  // 20초
const unsigned long WAIT_BEFORE_RETRY = 300000; // 5분 (300초)

void WiFiCheckTask(void *parameter)
{
  const unsigned long WIFI_CHECK_INTERVAL = 20000; // 20초마다 체크 (IDLE 상태에서만 사용)
  const int MAX_WIFI_RECONNECT_ATTEMPTS = 5;
  int reconnectAttempts = 0;
  int prevWiFiStatus = -1;
  static bool otaInitialized = false;
  unsigned long lastIdleCheck = 0;

  // 와치독 제거 (otacoEsp32IOT 방식)
  // esp_task_wdt_add(NULL);

  for (;;)
  {
    // 와치독 리셋 제거 (otacoEsp32IOT 방식)
    // esp_task_wdt_reset();

    int currStatus = WiFi.status();
    // 상태머신
    switch (wifiReconnectState)
    {
    case WIFI_IDLE:
      if (currStatus != WL_CONNECTED)
      {
        reconnectAttempts++;
        if (reconnectAttempts <= MAX_WIFI_RECONNECT_ATTEMPTS)
        {
          DEBUG_PRINT("[WiFiCheckTask] WiFi disconnected! Attempting reconnect (");
          DEBUG_PRINT(reconnectAttempts);
          DEBUG_PRINT("/");
          DEBUG_PRINT(MAX_WIFI_RECONNECT_ATTEMPTS);
          DEBUG_PRINTLN(")");
          currentSystemStatus = STATUS_WIFI_CONNECTING; // WiFi 연결 시도 상태
          WiFi.disconnect();
          vTaskDelay(200 / portTICK_PERIOD_MS); // 0.2초 대기 (FreeRTOS)
          WiFi.begin(ssid.c_str(), pass.c_str());
          reconnectStartTime = millis();
          wifiReconnectState = WIFI_RECONNECTING;
        }
        else
        {
          DEBUG_PRINTLN("[WiFiCheckTask] Max reconnect attempts reached. Waiting before retry...");
          currentSystemStatus = STATUS_OFFLINE; // 연결 실패 상태
          waitBeforeRetryStart = millis();
          wifiReconnectState = WIFI_WAIT_BEFORE_RETRY;
        }
      }
      else
      {
        // 연결된 상태에서는 5초마다 상태 출력
        if (millis() - lastIdleCheck > WIFI_CHECK_INTERVAL)
        {
          // DEBUG_PRINT("[WiFiCheckTask] WiFi.status(): ");
          // DEBUG_PRINTLN(currStatus);
          lastIdleCheck = millis();
        }
      }
      break;
    case WIFI_RECONNECTING:
      if (currStatus == WL_CONNECTED)
      {
        DEBUG_PRINTLN("[WiFiCheckTask] WiFi reconnected successfully! (detected by status change)");
        currentSystemStatus = STATUS_FIREBASE_CONNECTING; // Firebase 연결 시도 상태
        syncNTPTime(); // NTP 시간 동기화 호출
        DEBUG_PRINT("Current Time: ");
        DEBUG_PRINTLN(getCurrentTimeString());

        // === Firebase 초기화 및 연결 (재활성화) ===
        if (initFirebase())
        {
          DEBUG_PRINTLN("[WiFiCheckTask] Firebase initialized successfully!");
          
          // 장치 등록 시도
          DEBUG_PRINTLN("[WiFiCheckTask] About to call addDeviceIfNotExists...");
          DEBUG_PRINT("Device ID: ");
          DEBUG_PRINTLN(thingUniqueSerial);
          DEBUG_PRINT("Phone: ");
          DEBUG_PRINTLN(phone);

          bool result = addDeviceIfNotExists(thingUniqueSerial);

          DEBUG_PRINT("addDeviceIfNotExists result: ");
          DEBUG_PRINTLN(result ? "SUCCESS" : "FAILED");

          if (result)
          {
            DEBUG_PRINTLN("[WiFiCheckTask] Device registration successful!");
            // Firebase 스트림 연결은 FirebaseStreamTask에서 처리됨
            // 여기서는 STATUS_FIREBASE_CONNECTING 상태 유지
          }
          else
          {
            DEBUG_PRINTLN("[WiFiCheckTask] Device registration failed!");
            currentSystemStatus = STATUS_OFFLINE; // 장치 등록 실패 상태
          }
        }
        else
        {
          DEBUG_PRINTLN("[WiFiCheckTask] Firebase initialization failed!");
          currentSystemStatus = STATUS_OFFLINE; // Firebase 연결 실패 상태
        }

        if (!otaInitialized)
        {
          ArduinoOTA.setPassword("0221");
          ArduinoOTA.onStart([]()
                             { DEBUG_PRINTLN("Start updating..."); });
          ArduinoOTA.onEnd([]()
                           { DEBUG_PRINTLN("\nEnd"); });
          ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                                { DEBUG_PRINTF("Progress: %u%%\r", (progress / (total / 100))); });
          ArduinoOTA.onError([](ota_error_t error)
                             {
              DEBUG_PRINTF("Error[%u]: ", error);
              if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
              else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
              else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
              else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
              else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed"); });
          otaInitialized = true;
        }
        ArduinoOTA.begin();
        DEBUG_PRINTLN("Ready for OTA");
        DEBUG_PRINT("IP address: ");
        DEBUG_PRINTLN(WiFi.localIP());
        reconnectAttempts = 0;
        wifiReconnectState = WIFI_IDLE;
      }
      else if (millis() - reconnectStartTime > RECONNECT_TIMEOUT)
      {
        DEBUG_PRINTLN("[WiFiCheckTask] WiFi reconnect timeout. Will retry.");
        currentSystemStatus = STATUS_OFFLINE; // 연결 타임아웃 상태
        wifiReconnectState = WIFI_IDLE;
      }
      break;
    case WIFI_WAIT_BEFORE_RETRY:
      if (millis() - waitBeforeRetryStart > WAIT_BEFORE_RETRY)
      {
        reconnectAttempts = 0;
        wifiReconnectState = WIFI_IDLE;
      }
      break;
    }
    prevWiFiStatus = currStatus;
    vTaskDelay(500 / portTICK_PERIOD_MS); // 500ms마다 체크
  }
}

// 기존 Serial/Serial2 관련 polling, onReceive, 큐, 태스크, 버퍼, 인덱스, 플래그 등 모두 제거

// WiFi 정보 초기화 함수
void resetWiFiSettings()
{
  DEBUG_PRINTLN("=== resetWiFiSettings START ===");
  DEBUG_PRINTLN("Setting WiFi reset flag for next boot...");

  // WiFi 리셋 플래그 설정 (부팅 시 삭제 처리)
  preferences.begin("app-data", false);
  preferences.putBool("reset_wifi", true);
  preferences.end();

  DEBUG_PRINTLN("WiFi reset flag set successfully");
  DEBUG_PRINTLN("Rebooting to apply changes...");

  vTaskDelay(1000 / portTICK_PERIOD_MS); // 1초 대기 후 재부팅
  ESP.restart();
}

// NetworkInitTask 함수 구현 - WiFi 설정을 위한 AP 모드 및 웹 서버
void NetworkInitTask(void *parameter)
{
  DEBUG_PRINTLN("=== NetworkInitTask started ===");

  // 메모리 상태 확인
  DEBUG_PRINT("Free heap at start: ");
  DEBUG_PRINTLN(ESP.getFreeHeap());

  // 저장된 SSID가 있는지 확인
  if (ssid == "" || pass == "")
  {
    DEBUG_PRINTLN("No WiFi credentials found. Starting AP mode for initial setup.");
    // AP 모드로 전환
    isAPMode = true;
    currentSystemStatus = STATUS_AP_MODE; // AP 모드 상태 설정
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PulseFly WiFi Setup", NULL);

    IPAddress IP = WiFi.softAPIP();
    DEBUG_PRINT("AP IP address: ");
    DEBUG_PRINTLN(IP);

    // LittleFS 파일 목록 출력
    DEBUG_PRINTLN("Files in LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
      DEBUG_PRINT("  ");
      DEBUG_PRINT(file.name());
      DEBUG_PRINT("  ");
      DEBUG_PRINTLN(file.size());
      file = root.openNextFile();
    }

    // AP 모드용 웹 서버 설정
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                DEBUG_PRINTLN("Received request for /");
                if (!LittleFS.exists("/index.html")) {
                    DEBUG_PRINTLN("index.html not found in LittleFS!");
                    request->send(404, "text/plain", "File not found");
                    return;
                }
                DEBUG_PRINTLN("Sending index.html");
                request->send(LittleFS, "/index.html", "text/html"); });

    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                DEBUG_PRINTLN("Received request for style.css");
                if (!LittleFS.exists("/style.css")) {
                    DEBUG_PRINTLN("style.css not found in LittleFS!");
                    request->send(404, "text/plain", "File not found");
                    return;
                }
                DEBUG_PRINTLN("Sending style.css");
                request->send(LittleFS, "/style.css", "text/css"); });

    server.on("/success.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                DEBUG_PRINTLN("Received request for success.html");
                if (!LittleFS.exists("/success.html")) {
                    DEBUG_PRINTLN("success.html not found in LittleFS!");
                    request->send(404, "text/plain", "File not found");
                    return;
                }
                DEBUG_PRINTLN("Sending success.html");
                request->send(LittleFS, "/success.html", "text/html"); });

    server.serveStatic("/", LittleFS, "/");

    // 장치 ID를 제공하는 API 엔드포인트 추가
    server.on("/device-id", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                DEBUG_PRINTLN("Received request for device-id");
                request->send(200, "text/plain", thingUniqueSerial); });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        String savedSsid = "";
        String savedPhone = "";
        
        int params = request->params();
        for(int i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){
                if (p->name() == PARAM_INPUT_1) {
                    ssid = p->value().c_str();
                    savedSsid = ssid;
                    DEBUG_PRINT("SSID set to: ");
                    DEBUG_PRINTLN(ssid);
                    writeFile(LittleFS, ssidPath, ssid.c_str());
                }
                if (p->name() == PARAM_INPUT_2) {
                    pass = p->value().c_str();
                    DEBUG_PRINT("Password set to: ");
                    DEBUG_PRINTLN(pass);
                    writeFile(LittleFS, passPath, pass.c_str());
                }
                if (p->name() == PARAM_INPUT_3) {
                    phone = p->value().c_str();
                    savedPhone = phone;
                    DEBUG_PRINT("Phone number set to: ");
                    DEBUG_PRINTLN(phone);
                    writeFile(LittleFS, phonePath, phone.c_str());
                }
            }
        }
        
        // 성공 페이지로 리다이렉트 (설정값을 URL 파라미터로 전달)
        String redirectUrl = "/success.html?ssid=" + savedSsid + "&phone=" + savedPhone;
        request->redirect(redirectUrl);
        
        DEBUG_PRINTLN("Settings saved, redirecting to success page");
        
        // 재부팅 플래그 설정 (별도 태스크에서 처리)
        shouldReboot = true;
        rebootTime = millis();
        DEBUG_PRINTLN("Reboot scheduled in 8 seconds"); });

    server.begin();
    DEBUG_PRINTLN("Device is in AP mode. Please connect to 'ESP32PulseFly Setup' network.");
  }
  else
  {
    DEBUG_PRINTLN("WiFi credentials found. Attempting to connect...");
    currentSystemStatus = STATUS_WIFI_CONNECTING; // WiFi 연결 시도 상태로 변경
    // WiFi 연결 시도는 WiFiCheckTask에서 처리됨
  }

  // 태스크 종료
  vTaskDelete(NULL);
}

// 버튼 입력 처리 태스크
void ButtonInputTask(void *parameter)
{
  while (true)
  {
    // 현재 버튼 상태 읽기
    int reading = digitalRead(BTN_USER);

    // 버튼 상태가 변경되었는지 확인
    if (reading != lastButtonState)
    {
      // 디바운스 타이머 리셋
      lastDebounceTime = millis();
    }

    // 디바운스 지연 시간이 지났는지 확인
    if ((millis() - lastDebounceTime) > debounceDelay)
    {
      // 버튼 상태가 실제로 변경되었는지 확인
      if (reading != buttonState)
      {
        buttonState = reading;

        // 버튼이 눌렸을 때 (LOW 상태)
        if (buttonState == LOW)
        {
          buttonPressStartTime = millis(); // 버튼 누른 시간 기록
          isLongPress = false;             // 길게 누르기 상태 초기화
          DEBUG_PRINTLN("Button pressed!");

          digitalWrite(LED_INFO, HIGH); // 끄기
        }
        // 버튼이 떼어졌을 때 (HIGH 상태)
        else
        {
          buttonPressStartTime = 0; // 버튼 누른 시간 초기화
        }
      }
    }

    // 버튼이 눌려있는 상태에서 5초가 지났는지 확인
    if (buttonState == LOW && !isLongPress)
    {
      if (millis() - buttonPressStartTime >= 5000)
      { // 5초
        DEBUG_PRINTLN("Button long pressed for 5 seconds!");
        DEBUG_PRINTLN("Resetting WiFi settings...");

        digitalWrite(LED_INFO, LOW); // 켜기
        resetWiFiSettings();
        isLongPress = true; // 길게 누르기 상태 설정
      }
    }

    // 버튼 상태 저장
    lastButtonState = reading;

    vTaskDelay(50 / portTICK_PERIOD_MS); // 50ms 딜레이 (버튼 디바운스용)
  }
}

// 지연 재부팅 처리 태스크
void RebootTask(void *parameter)
{
  while (true)
  {
    // 재부팅 플래그가 설정되어 있고 시간이 지났는지 확인
    if (shouldReboot && (millis() - rebootTime >= REBOOT_DELAY))
    {
      DEBUG_PRINTLN("=== Scheduled reboot executing ===");
      DEBUG_PRINTLN("Rebooting device after WiFi configuration...");

      // 재부팅 플래그 초기화
      shouldReboot = false;

      // 1초 대기 후 재부팅
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      ESP.restart();
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS); // 1초마다 체크
  }
}

bool initLittleFS()
{
  if (!LittleFS.begin(true))
  {
    DEBUG_PRINTLN("An error has occurred while mounting LittleFS");
    return false;
  }
  DEBUG_PRINTLN("LittleFS mounted successfully");
  return true;
}

void printResetReason()
{
  esp_reset_reason_t reason = esp_reset_reason();
  DEBUG_PRINT("[RESET REASON] ");
  switch (reason)
  {
  case ESP_RST_POWERON:
    DEBUG_PRINTLN("Power-on");
    break;
  case ESP_RST_EXT:
    DEBUG_PRINTLN("External reset");
    break;
  case ESP_RST_SW:
    DEBUG_PRINTLN("Software reset");
    break;
  case ESP_RST_PANIC:
    DEBUG_PRINTLN("Exception/panic");
    break;
  case ESP_RST_INT_WDT:
    DEBUG_PRINTLN("Interrupt watchdog");
    break;
  case ESP_RST_TASK_WDT:
    DEBUG_PRINTLN("Task watchdog");
    break;
  case ESP_RST_WDT:
    DEBUG_PRINTLN("Other watchdog");
    break;
  case ESP_RST_DEEPSLEEP:
    DEBUG_PRINTLN("Wake from deep sleep");
    break;
  case ESP_RST_BROWNOUT:
    DEBUG_PRINTLN("Brownout");
    break;
  case ESP_RST_SDIO:
    DEBUG_PRINTLN("SDIO reset");
    break;
  default:
    DEBUG_PRINTLN("Unknown");
    break;
  }
}

void setup()
{

#pragma region GPIO 설정
  pinMode(LED_WHITE_BUILTIN, OUTPUT);
  digitalWrite(LED_WHITE_BUILTIN, LOW);

  pinMode(LED_INFO, OUTPUT);
  digitalWrite(LED_INFO, HIGH); // Turn off

  pinMode(LED_STATUS, OUTPUT);
  digitalWrite(LED_STATUS, LOW); // 부팅 시 LED 켜기 (STATUS_BOOTING 상태)

  pinMode(PULSE_OUT, OUTPUT);
  digitalWrite(PULSE_OUT, LOW);

  // 버튼 핀 초기화 (내부 풀업 저항 사용)
  pinMode(BTN_USER, INPUT_PULLUP);

  digitalWrite(LED_WHITE_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_WHITE_BUILTIN, LOW);
#pragma endregion

#pragma region 시리얼통신 초기화
  // IO UART1 설정 (TX=19, RX=18, 19200)
  uart_config_t io_uart_config = {
      .baud_rate = 19200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  uart_param_config(IO_UART, &io_uart_config);
  uart_set_pin(IO_UART, 19, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(IO_UART, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

  // BILL UART2 설정 (TX=17, RX=16, 19200)
  uart_config_t bill_uart_config = {
      .baud_rate = 19200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
  uart_param_config(BILL_UART, &bill_uart_config);
  uart_set_pin(BILL_UART, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(BILL_UART, UART_BUF_SIZE * 2, 0, 0, NULL, 0);

  Serial.begin(115200); //
  while (!Serial)
  {
    delay(10);
  }

  DEBUG_PRINTLN("     ########## RESET ##########     ");
  printResetReason();

#pragma region Memory Diagnostic Tools Activation
  // Heap status output (using only functions available in ESP32)
  heap_caps_print_heap_info(MALLOC_CAP_8BIT);

  // Memory leak detection
  DEBUG_PRINTLN("=== Memory Diagnostic Start ===");
  DEBUG_PRINT("Free Heap: ");
  DEBUG_PRINTLN(ESP.getFreeHeap());
  DEBUG_PRINT("Min Free Heap: ");
  DEBUG_PRINTLN(ESP.getMinFreeHeap());
  DEBUG_PRINT("Max Alloc Heap: ");
  DEBUG_PRINTLN(ESP.getMaxAllocHeap());
  DEBUG_PRINT("Heap Size: ");
  DEBUG_PRINTLN(ESP.getHeapSize());
  DEBUG_PRINTLN("=============================");

  // 메모리 초기화 강화
  if (ESP.getFreeHeap() < 50000) // 50KB 미만이면 경고
  {
    DEBUG_PRINTLN("⚠️  WARNING: Low initial memory! Free heap: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());
  }
#pragma endregion

#pragma endregion

#pragma region SSL/TLS 설정 초기화
  // SSL/TLS 설정 강화
  DEBUG_PRINTLN("Initializing SSL/TLS settings...");
  // SSL 설정은 WiFiClientSecure에서 자동으로 처리됨
  DEBUG_PRINTLN("SSL/TLS settings will be configured automatically");
#pragma endregion

#pragma region 칩아이디 가져오기
  DEBUG_PRINTLN("Getting Chip ID...");
  thingUniqueSerial = getChipID(); // thingUniqueSerial 처음 사용
  DEBUG_PRINT("Chip ID: ");
  DEBUG_PRINTLN(thingUniqueSerial);
#pragma endregion

#pragma region 저장된 카운터 값 읽기 및 출력
  readData(countA, countB, pulseA, pulseB);
  DEBUG_PRINTLN("Saved Counter Values:");
  DEBUG_PRINT("countA: ");
  DEBUG_PRINTLN(countA);
  DEBUG_PRINT("countB: ");
  DEBUG_PRINTLN(countB);
  DEBUG_PRINT("pulseA: ");
  DEBUG_PRINTLN(pulseA);
  DEBUG_PRINT("pulseB: ");
  DEBUG_PRINTLN(pulseB);
#pragma endregion

#pragma region 리틀FS에서 설정값 가져오기
  initLittleFS();

  // WiFi 리셋 플래그 체크 (부팅 시 처리)
  preferences.begin("app-data", true);
  bool shouldResetWiFi = preferences.getBool("reset_wifi", false);
  preferences.end();

  if (shouldResetWiFi)
  {
    DEBUG_PRINTLN("=== WiFi Reset Flag Detected ===");
    DEBUG_PRINTLN("Deleting WiFi configuration files during boot...");

    // 안전하게 파일 삭제 (다른 태스크들이 아직 시작되지 않은 상태)
    if (LittleFS.exists(ssidPath))
    {
      bool result = LittleFS.remove(ssidPath);
      DEBUG_PRINT("SSID file deletion: ");
      DEBUG_PRINTLN(result ? "SUCCESS" : "FAILED");
    }

    if (LittleFS.exists(passPath))
    {
      bool result = LittleFS.remove(passPath);
      DEBUG_PRINT("Password file deletion: ");
      DEBUG_PRINTLN(result ? "SUCCESS" : "FAILED");
    }

    if (LittleFS.exists(phonePath))
    {
      bool result = LittleFS.remove(phonePath);
      DEBUG_PRINT("Phone file deletion: ");
      DEBUG_PRINTLN(result ? "SUCCESS" : "FAILED");
    }

    // 플래그 제거
    preferences.begin("app-data", false);
    preferences.remove("reset_wifi");
    preferences.end();

    DEBUG_PRINTLN("WiFi configuration files deleted successfully");
    DEBUG_PRINTLN("=== WiFi Reset Completed ===");

    // 변수를 빈 값으로 초기화
    ssid = "";
    pass = "";
    phone = "";
  }
  else
  {
    // 정상적으로 파일에서 설정값 읽기
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    phone = readFile(LittleFS, phonePath);
  }

  DEBUG_PRINT("SSID: ");
  DEBUG_PRINTLN(ssid);
  DEBUG_PRINT("Password: ");
  DEBUG_PRINTLN(pass);
  DEBUG_PRINT("Phone: ");
  DEBUG_PRINTLN(phone);
#pragma endregion

#pragma region WiFi 필수 초기화 (연결은 하지 않음)
  WiFi.mode(WIFI_STA);   // 스테이션 모드로 설정
  WiFi.disconnect(true); // 이전 연결 정보/캐시 삭제 및 초기화
  delay(10);             // 하드웨어 안정화 대기
#pragma endregion

#pragma region 시리얼 통신 태스크 생성 (serialEvent, serialEvent2를 polling)
  // SerialCommuteTask는 Core 1에 고정 (우선순위 최고로 설정)
  // xTaskCreatePinnedToCore(
  //     SerialCommuteTask,
  //     "SerialCommuteTask",
  //     4096,
  //     NULL,
  //     5, // 우선순위 5로 높임 (최고 우선순위)
  //     &SerialCommuteTaskHandle,
  //     1 // Core 1
  // );
  // xTaskCreatePinnedToCore(
  //     Serial2CommuteTask,
  //     "Serial2CommuteTask",
  //     4096,
  //     NULL,
  //     5, // 우선순위 5로 높임 (최고 우선순위)
  //     &Serial2CommuteTaskHandle,
  //     1 // Core 1
  // );
#pragma endregion

#pragma region WiFi 설정 및 초기화 태스크 생성
  // NetworkInitTask는 Core 0에 고정 (WiFi 설정을 위한 AP 모드)
  xTaskCreatePinnedToCore(
      NetworkInitTask,
      "NetworkInitTask",
      8192, // 스택 크기
      NULL,
      2, // 높은 우선순위 (WiFiCheckTask보다 높음)
      NULL,
      0 // Core 0
  );
  DEBUG_PRINTLN("NetworkInitTask started for WiFi configuration");
#pragma endregion

#pragma region WiFi 상태 체크 및 자동 재연결 태스크 생성
  // WiFiCheckTask는 Core 0에 고정
  xTaskCreatePinnedToCore(
      WiFiCheckTask,
      "WiFiCheckTask",
      8192, // 스택 크기 증가
      NULL,
      1,
      NULL,
      0 // Core 0
  );
#pragma endregion

#pragma region Firebase 스트림 태스크 생성
  // Firebase 스트림 태스크 재활성화 (스트림 모드)
  xTaskCreatePinnedToCore(
      FirebaseStreamTask,
      "FirebaseStreamTask",
      16384, // 스택 크기 증가
      NULL,
      1, // 낮은 우선순위
      &FirebaseStreamTaskHandle,
      0 // Core 0
  );
  DEBUG_PRINTLN("FirebaseStreamTask started in Stream Mode");
#pragma endregion

#pragma region 버튼 입력 처리 태스크 생성
  // ButtonInputTask는 Core 0에 고정
  xTaskCreatePinnedToCore(
      ButtonInputTask,   // 실행할 함수
      "ButtonInputTask", // 태스크 이름
      2048,              // 스택 크기 (바이트)
      NULL,              // 전달할 매개변수
      3,                 // 우선순위 (LED 태스크들보다 높음)
      NULL,              // 태스크 핸들
      0                  // Core 0
  );
  DEBUG_PRINTLN("ButtonInputTask started for WiFi reset functionality");
#pragma endregion

#pragma region 재부팅 처리 태스크 생성
  // RebootTask는 Core 0에 고정
  xTaskCreatePinnedToCore(
      RebootTask,   // 실행할 함수
      "RebootTask", // 태스크 이름
      2048,         // 스택 크기 (바이트)
      NULL,         // 전달할 매개변수
      1,            // 우선순위 (낮음)
      NULL,         // 태스크 핸들
      0             // Core 0
  );
  DEBUG_PRINTLN("RebootTask started for delayed reboot functionality");
#pragma endregion

#pragma region LED 상태 표시 태스크 생성
  // LEDStatusTask는 Core 0에 고정
  xTaskCreatePinnedToCore(
      LEDStatusTask,   // 실행할 함수
      "LEDStatusTask", // 태스크 이름
      2048,            // 스택 크기 (바이트)
      NULL,            // 전달할 매개변수
      2,               // 우선순위
      NULL,            // 태스크 핸들
      0                // Core 0
  );

  // LEDBlinkTask는 Core 0에 고정 (otacoEsp32IOT 참고)
  xTaskCreatePinnedToCore(
      LEDBlinkTask,   // 실행할 함수
      "LEDBlinkTask", // 태스크 이름
      2048,           // 스택 크기 (바이트)
      NULL,           // 전달할 매개변수
      3,              // 우선순위 (LEDStatusTask보다 높음)
      NULL,           // 태스크 핸들
      0               // Core 0
  );
#pragma endregion

#pragma region 와치독 초기화
  esp_task_wdt_init(180, true); // 타임아웃 180초로 증가, 패닉 발생
  esp_task_wdt_add(NULL);       // 현재 태스크 추가
#pragma endregion

  // 큐 생성
  ioUartQueue = xQueueCreate(10, sizeof(char[5])); // 큐 크기 10, 데이터 크기 5
  if (ioUartQueue == NULL)
  {
    DEBUG_PRINTLN("Failed to create ioUartQueue");
  }
  billUartQueue = xQueueCreate(10, sizeof(char[5])); // 큐 크기 10, 데이터 크기 5
  if (billUartQueue == NULL)
  {
    DEBUG_PRINTLN("Failed to create billUartQueue");
  }

  // 태스크 생성
  xTaskCreate(io_uart_task, "io_uart_task", 2048, NULL, 1, NULL);
  xTaskCreate(bill_uart_task, "bill_uart_task", 2048, NULL, 1, NULL);
  xTaskCreate(BusinessLogicTask, "business_logic_task", 4096, NULL, 2, NULL);

} // setup()

void loop()
{
  ArduinoOTA.handle();
  esp_task_wdt_reset();

  // Memory usage monitoring (every 30 seconds)
  static unsigned long lastMemoryCheck = 0;
  if (millis() - lastMemoryCheck > 30000)
  {
    lastMemoryCheck = millis();

    DEBUG_PRINTLN("=== Memory Status Monitoring ===");
    DEBUG_PRINT("Free Heap: ");
    DEBUG_PRINTLN(ESP.getFreeHeap());
    DEBUG_PRINT("Min Free Heap: ");
    DEBUG_PRINTLN(ESP.getMinFreeHeap());
    DEBUG_PRINT("Max Alloc Heap: ");
    DEBUG_PRINTLN(ESP.getMaxAllocHeap());
    DEBUG_PRINTLN("===========================");

    // Memory shortage warning and recovery (더 보수적인 임계값)
    if (ESP.getFreeHeap() < 50000) // 50KB로 임계값 높임
    {
      DEBUG_PRINTLN("⚠️  WARNING: Low memory! Free heap: ");
      DEBUG_PRINTLN(ESP.getFreeHeap());
      DEBUG_PRINTLN("Performing memory cleanup...");

      // 메모리 정리 시도
      vTaskDelay(3000 / portTICK_PERIOD_MS); // 3초 대기

      // 가비지 컬렉션 유도
      String temp = "";
      temp.reserve(1000);
      temp = "";

      // 추가 메모리 정리
      for (int i = 0; i < 10; i++)
      {
        String cleanup = "";
        cleanup.reserve(100);
        cleanup = "";
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      DEBUG_PRINTLN("Memory cleanup completed. Free heap: ");
      DEBUG_PRINTLN(ESP.getFreeHeap());
    }

    // Critical memory shortage (더 보수적인 임계값)
    if (ESP.getFreeHeap() < 30000) // 30KB 미만이면 재부팅
    {
      DEBUG_PRINTLN("🚨 CRITICAL: Very low memory! Free heap: ");
      DEBUG_PRINTLN(ESP.getFreeHeap());
      DEBUG_PRINTLN("Rebooting due to critical memory shortage...");
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      ESP.restart();
    }
  }
}
