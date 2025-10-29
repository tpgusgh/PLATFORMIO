#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define LED_PIN1 32
#define LED_PIN2 33
#define BUTTON_PIN 14

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t broadcastAddress[] = {0x88, 0x85, 0x27, 0x11, 0x55, 0x45};

typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

struct_message myData;

esp_now_peer_info_t peerInfo; //데이터 송수신 할 그룹 지정할때 필요

// Declare task handle
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;
TaskHandle_t TaskButtonHandle= NULL;
uint8_t baseMac[6];

void readMacAddress() {
  uint8_t mac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if(ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                baseMac[0], baseMac[1], baseMac[2],
                baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to get MAC address");
  };
}

void onDataRecv(const uint8_t * mac, const uint8_t * incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Char : ");
}
void Task1(void *parameter) {
  for (;;) { // Infinite loop
    digitalWrite(LED_PIN1, HIGH);
    Serial.println("Task1: LED ON");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000ms

    digitalWrite(LED_PIN1, LOW);
    Serial.println("Task1: LED OFF");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    Serial.printf("Task1 Stack Free: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));

    // ESP-NOW 통신
    strcpy(myData.a, "ESP-NOW TEST SEND");
    myData.b = random(0, 100);
    myData.c = 1.2;
    myData.d = false;

    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    if(result == ESP_OK) {
      Serial.println("Sent with success");
    } else {
      Serial.println("Error sending the data");
    }
  }
}

void Task2(void *parameter) {
  for (;;) { // Infinite loop
    digitalWrite(LED_PIN2, HIGH);
    Serial.println("Task2: LED ON");
    vTaskDelay(500 / portTICK_PERIOD_MS); // 500ms

    digitalWrite(LED_PIN2, LOW);
    Serial.println("Task2: LED OFF");
    vTaskDelay(500 / portTICK_PERIOD_MS);
   
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());

    Serial.printf("Task2 Stack Free: %u bytes\n", uxTaskGetStackHighWaterMark(NULL));
  }
}
void TaskButton(void *parameter) {
  for (;;){
  if(digitalRead(BUTTON_PIN) == HIGH){
  vTaskSuspend(Task1Handle);
  }
  if(digitalRead(BUTTON_PIN) == HIGH){
  vTaskResume(Task1Handle);
}
}
}
void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.printf("Starting FreeRTOS: Memory Usage\nInitial Free Heap: %u bytes\n", xPortGetFreeHeapSize());
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  Serial.println("ESP32 Board MAC Address: ");
  readMacAddress();
//1 init esp-now
  if(esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
//2 register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
//3 add peer
  if(esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
//4 recive callback
  esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));

  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(BUTTON_PIN, OUTPUT);
  xTaskCreatePinnedToCore(
    Task1,         // Task function
    "Task1",       // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameters
    1,                 // Priority
    &Task1Handle,  // Task handle
    1                  // Core 1
  );

    xTaskCreatePinnedToCore(
    Task2,         // Task function
    "Task2",       // Task name
    10000,             // Stack size (bytes)
    NULL,              // Parameters
    1,                 // Priority
    &Task2Handle,  // Task handle
    1                  // Core 1
  );
}

void loop() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    Serial.printf("Free Heap: %u bytes\n", xPortGetFreeHeapSize());
    lastCheck = millis();


  }
}