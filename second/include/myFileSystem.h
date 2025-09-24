#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H

#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include <ArduinoJson.h>

class MyFileSystem
{
public:
    static bool initialize();
    static void printInfo();
    static bool createTestFile();
    static bool readTestFile();
    static void listFiles();
    static void printTree(const char *dirname = "/", uint8_t levels = 3);
    static size_t getTotalBytes();
    static size_t getUsedBytes();
    static size_t getFreeBytes();

    // JSON configuration
    static bool loadConfig();
    static bool saveConfig();
    static void printConfig();
    static String getConfigValue(const char *key);
    static bool setConfigValue(const char *key, const char *value);

    // WiFi helpers
    static String getWiFiSSID();
    static String getWiFiPassword();
    static int getWiFiTimeout();
    static bool setWiFiCredentials(const char* ssid, const char* password);

private:
    static void printTreeRecursive(fs::FS &fs, const char *dirname, uint8_t levels, uint8_t indent);

    // DynamicJsonDocument 사용 (protected 문제 해결)
    static DynamicJsonDocument configDoc;
};

#endif
