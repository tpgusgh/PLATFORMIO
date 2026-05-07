#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <MQTTPubSubClient.h>

const char* ssid = "bssm_free";
const char* password = "bssm_free";
const char* host = "aibrokerweb.mieung.kr";
const uint16_t port = 443;

WebSocketsClient webSocket;
MQTTPubSubClient mqtt;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected");

    webSocket.beginSSL(host, port, "/"); 
    webSocket.setReconnectInterval(5000);
    webSocket.enableHeartbeat(10000, 3000, 2); 

    mqtt.begin(webSocket);
}

void loop() {
    webSocket.loop();
    mqtt.update();

    if (!mqtt.isConnected()) {
        static uint32_t last_reconnect = 0;
        if (millis() - last_reconnect > 5000) {
            last_reconnect = millis();
            if (mqtt.connect("ESP32_Sensor_Node", "", "")) {
                Serial.println(">>> Broker Connected!");
            }
        }
    }

    // 5초마다 'grad/project' 토픽으로 데이터 발행
    static uint32_t last_publish = 0;
    if (mqtt.isConnected() && millis() - last_publish > 5000) {
        last_publish = millis();
        String message = "Sensor Data: " + String(random(20, 30)) + "C";
        mqtt.publish("grad/project", message.c_str());
        Serial.println("Published: " + message);
    }
}