#include <Arduino.h>

#define LED_PIN1 32
#define LED_PIN2 33
#define BUTTON_PIN 14

// Declare task handle
TaskHandle_t Task1Handle = NULL;
TaskHandle_t Task2Handle = NULL;
TaskHandle_t TaskButtonHandle = NULL;

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

void TaskButton(void *paramter) {
  for (;;) { // Infinite loop
    // digitalRead(BUTTON_PIN)
    버튼 인식하면
      vTaskSuspend(Task1Handle);

    버튼 인식하면
      vTaskResume(Task1Handle);
  }
}

void setup() {
  Serial.begin(115200);

  delay(1000);
  Serial.printf("Starting FreeRTOS: Memory Usage\nInitial Free Heap: %u bytes\n", xPortGetFreeHeapSize());
 
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

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