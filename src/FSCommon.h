#pragma once

// ESP32 version
#include "LittleFS.h"
#define FSCom LittleFS
#define FSBegin() FSCom.begin(true)
#define FILE_O_WRITE "w"
#define FILE_O_READ "r"

void fsInit();
void listDir(const char * dirname, uint8_t levels, boolean del);
