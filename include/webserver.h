#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdbool.h>
#include <Arduino.h>

#include "config.h"

// bool EspReboot;

String checkLinkStatus(void);

String processor(const String& var);
void configureWebServer(AsyncWebServer *server);
void notFound(AsyncWebServerRequest *request);
bool checkUserWebAuth(AsyncWebServerRequest * request);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

#endif // WEBSERVER_H
