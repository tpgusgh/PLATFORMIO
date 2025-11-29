// util.cpp

#include "util.h"

Preferences preferences;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 32400; // 한국 시간대 (UTC+9)
const int daylightOffset_sec = 0; // 일광절약시간 적용 안함

char *getChipID()
{
  // Get the chip ID.
  uint64_t chipid = ESP.getEfuseMac();

  // Convert the chip ID to a string.
  char chipid_str[17]; // 16자리 + NULL 문자
  snprintf(chipid_str, sizeof(chipid_str), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

  // Store the string in dynamic memory.
  return strdup(chipid_str);
}

String generateRandomString(int length)
{
  String randomString = "";
  char randomChar;

  for (int i = 0; i < length; i++)
  {
    int randomChoice = random(0, 3); // 숫자, 소문자, 대문자 중 선택

    if (randomChoice == 0)
    {
      // 숫자 (0~9)
      randomChar = char(random(48, 58)); // 아스키 코드: '0' (48) ~ '9' (57)
    }
    else if (randomChoice == 1)
    {
      // 대문자 (A~Z)
      randomChar = char(random(65, 91)); // 아스키 코드: 'A' (65) ~ 'Z' (90)
    }
    else
    {
      // 소문자 (a~z)
      randomChar = char(random(97, 123)); // 아스키 코드: 'a' (97) ~ 'z' (122)
    }

    randomString += randomChar; // 문자열에 추가
  }

  return randomString; // 최종 문자열 리턴
}

void syncNTPTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    DEBUG_PRINTLN("NTP sync failed!");
  }
  else
  {
    DEBUG_PRINTLN("NTP sync success!");
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    DEBUG_PRINT("Current time: ");
    DEBUG_PRINTLN(timeStringBuff);
  }
}

String getCurrentTimeString()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    DEBUG_PRINTLN("Failed to obtain time");
    return "";
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

// URL 인코딩 함수 구현
String urlEncode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// 16진수 문자를 정수로 변환하는 헬퍼 함수
int hexCharToInt(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  else if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }
  else if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }
  return 0;
}

// URL 디코딩 함수 구현
String urlDecode(String str)
{
  String decodedString = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == '+')
    {
      decodedString += ' ';
    }
    else if (c == '%')
    {
      if (i + 2 < str.length())
      {
        code0 = str.charAt(i + 1);
        code1 = str.charAt(i + 2);
        if (isxdigit(code0) && isxdigit(code1))
        {
          c = (hexCharToInt(code0) << 4) | hexCharToInt(code1);
          decodedString += c;
          i += 2;
        }
        else
        {
          decodedString += c;
        }
      }
      else
      {
        decodedString += c;
      }
    }
    else
    {
      decodedString += c;
    }
  }
  return decodedString;
}

// 파일 쓰기 함수 구현
bool writeFile(fs::FS &fs, const char *path, const char *message)
{
  DEBUG_PRINTF("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    DEBUG_PRINTLN("- failed to open file for writing");
    return false;
  }
  if (file.print(message))
  {
    DEBUG_PRINTLN("- file written");
    return true;
  }
  else
  {
    DEBUG_PRINTLN("- write failed");
    return false;
  }
}

// 파일 읽기 함수 구현
String readFile(fs::FS &fs, const char *path)
{
  DEBUG_PRINTF("Reading file: %s\n", path);

  File file = fs.open(path, FILE_READ);
  if (!file)
  {
    DEBUG_PRINTLN("- file does not exist");
    return String();
  }
  String content = file.readString();
  file.close();
  return content;
}

void readData(uint16_t &countA, uint16_t &countB, uint16_t &pulseA, uint16_t &pulseB)
{
  preferences.begin("app-data", true);         // Start with the namespace "app-data" (read-only).
  countA = preferences.getUShort("countA", 0); // Read the value of countA, default is 0.
  countB = preferences.getUShort("countB", 0); // Read the value of countB, default is 0.
  pulseA = preferences.getUShort("pulseA", 0); // Read the value of pulseA, default is 0.
  pulseB = preferences.getUShort("pulseB", 0); // Read the value of pulseB, default is 0.
  preferences.end();                           // End flash memory.
}

void saveData(uint16_t countA, uint16_t countB, uint16_t pulseA, uint16_t pulseB)
{
  preferences.begin("app-data", false);    // Start with the namespace "app-data".
  preferences.putUShort("countA", countA); // Save countA.
  preferences.putUShort("countB", countB); // Save countB.
  preferences.putUShort("pulseA", pulseA); // Save pulseA.
  preferences.putUShort("pulseB", pulseB); // Save pulseB.
  preferences.end();                       // End flash memory.

  readData(countA, countB, pulseA, pulseB);
}