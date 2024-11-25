
#ifndef UTILS_H
#define UTILS_H


#include <stdbool.h>
#include <Arduino.h>
#include <SPIFFS.h>

void rebootESP(String message);
String listFiles(bool ishtml);
String humanReadableSize(const size_t bytes);

#endif // UTILS_H
