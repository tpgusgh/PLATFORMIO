#include "myFileSystem.h"

// DynamicJsonDocument 초기화 (1KB)
DynamicJsonDocument MyFileSystem::configDoc(1024);

bool MyFileSystem::initialize()
{
    Serial.println("=== ESP32S3 LittleFS Test ===");
    if (!LittleFS.begin(false))
    {
        Serial.println("LittleFS mount failed, formatting...");
        if (!LittleFS.format())
        {
            Serial.println("Format failed!");
            return false;
        }
        return LittleFS.begin(false);
    }
    Serial.println("LittleFS mount successful!");
    return true;
}

void MyFileSystem::printInfo()
{
    Serial.printf("Total: %d, Used: %d, Free: %d\n", getTotalBytes(), getUsedBytes(), getFreeBytes());
}

bool MyFileSystem::createTestFile()
{
    File file = LittleFS.open("/test.txt", "w");
    if (!file) return false;
    file.println("Hello LittleFS!");
    file.printf("Time: %lu ms\n", millis());
    file.close();
    return true;
}

bool MyFileSystem::readTestFile()
{
    File file = LittleFS.open("/test.txt", "r");
    if (!file) return false;
    while(file.available()) Serial.write(file.read());
    file.close();
    return true;
}

void MyFileSystem::listFiles()
{
    File root = LittleFS.open("/");
    File file_item = root.openNextFile();
    while(file_item)
    {
        Serial.printf("%s (%d bytes)\n", file_item.name(), file_item.size());
        file_item = root.openNextFile();
    }
    root.close();
}

void MyFileSystem::printTree(const char *dirname, uint8_t levels)
{
    printTreeRecursive(LittleFS, dirname, levels, 0);
}

void MyFileSystem::printTreeRecursive(fs::FS &fs, const char *dirname, uint8_t levels, uint8_t indent)
{
    File root = fs.open(dirname);
    if(!root || !root.isDirectory()) { if(root) root.close(); return; }

    File entry = root.openNextFile();
    while(entry)
    {
        for(uint8_t i=0;i<indent;i++) Serial.print("  ");
        Serial.print("|- "); Serial.print(entry.name());
        if(!entry.isDirectory()) Serial.printf(" (%d bytes)", entry.size());
        Serial.println();
        if(entry.isDirectory() && levels>0) printTreeRecursive(fs, entry.name(), levels-1, indent+1);
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
}

size_t MyFileSystem::getTotalBytes() { return LittleFS.totalBytes(); }
size_t MyFileSystem::getUsedBytes() { return LittleFS.usedBytes(); }
size_t MyFileSystem::getFreeBytes() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }

bool MyFileSystem::loadConfig()
{
    File file = LittleFS.open("/config.txt", "r");
    if(!file) return false;
    configDoc.clear();
    DeserializationError error = deserializeJson(configDoc, file);
    file.close();
    return !error;
}

bool MyFileSystem::saveConfig()
{
    File file = LittleFS.open("/config.txt", "w");
    if(!file) return false;
    serializeJson(configDoc, file);
    file.close();
    return true;
}

void MyFileSystem::printConfig()
{
    serializeJsonPretty(configDoc, Serial);
    Serial.println();
}

String MyFileSystem::getConfigValue(const char* key)
{
    if(configDoc.containsKey(key)) return configDoc[key].as<String>();
    return "";
}

bool MyFileSystem::setConfigValue(const char* key, const char* value)
{
    configDoc[key] = value;
    return saveConfig();
}

String MyFileSystem::getWiFiSSID()
{
    if(configDoc.containsKey("wifi") && configDoc["wifi"].containsKey("ssid"))
        return configDoc["wifi"]["ssid"].as<String>();
    return "";
}

String MyFileSystem::getWiFiPassword()
{
    if(configDoc.containsKey("wifi") && configDoc["wifi"].containsKey("password"))
        return configDoc["wifi"]["password"].as<String>();
    return "";
}

bool MyFileSystem::setWiFiCredentials(const char* ssid, const char* password)
{
    if(!configDoc.containsKey("wifi")) configDoc["wifi"] = JsonObject();
    configDoc["wifi"]["ssid"] = ssid;
    configDoc["wifi"]["password"] = password;
    configDoc["wifi"]["timeout"] = 10000;
    return saveConfig();
}

int MyFileSystem::getWiFiTimeout()
{
    if(configDoc.containsKey("wifi") && configDoc["wifi"].containsKey("timeout"))
        return configDoc["wifi"]["timeout"].as<int>();
    return 10000;
}
