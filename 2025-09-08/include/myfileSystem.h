#ifndef MYFILESYSTEM_H
#define MYFILESYSTEM_H


#include <Arduino.h>
#include <LittleFS.h>

bool initLittleFS();

bool writeLittleFS();

bool readLittleFS();

#endif

