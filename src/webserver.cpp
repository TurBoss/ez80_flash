#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <Arduino.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "webserver.h"
#include "webpages.h"
#include "utils.h"
#include "tool.h"

#define FIRMWARE_VERSION "v0.0.1"

HttpConfig http_config;                        // configuration

String httpuser = "admin";
String httppassword = "admin";

bool cpu_reset = false;
bool uplink;


// parses and processes webpages
// if the webpage has %SOMETHING% or %SOMETHINGELSE% it will replace those strings with the ones defined
String processor(const String& var) {
	if (var == "FIRMWARE") {
		return FIRMWARE_VERSION;
	}

	if (var == "FREESPIFFS") {
		return humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
	}

	if (var == "USEDSPIFFS") {
		return humanReadableSize(SPIFFS.usedBytes());
	}

	if (var == "TOTALSPIFFS") {
		return humanReadableSize(SPIFFS.totalBytes());
	}
	return "";
}

void configureWebServer(AsyncWebServer *server) {
	// configure web server

	// if url isn't found
	server->onNotFound(notFound);

	// run handleUpload function when any file is uploaded
	server->onFileUpload(handleUpload);

	// visiting this page will cause you to be logged out
	server->on("/logout", HTTP_GET, [](AsyncWebServerRequest * request) {
		request->requestAuthentication();
		request->send(401);
	});

	// presents a "you are now logged out webpage
	server->on("/logged-out", HTTP_GET, [](AsyncWebServerRequest * request) {
		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		// Serial.println(logmessage);
		request->send(401, "text/html", logout_html, processor);
	});

	server->on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
		String logmessage = "Client:" + request->client()->remoteIP().toString() + + " " + request->url();

		if (checkUserWebAuth(request)) {
			logmessage += " Auth: Success";
			// Serial.println(logmessage);
			request->send(200, "text/html", index_html, processor);
		} else {
			logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}

	});

	server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest * request) {
		//String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		if (checkUserWebAuth(request)) {
			request->send(200, "text/html", "");
			// logmessage += " Auth: Success";
			// Serial.println(logmessage);
			resetCPU();
		} else {
			// logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}
	});

	server->on("/listFiles", HTTP_GET, [](AsyncWebServerRequest * request)
			{
		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		if (checkUserWebAuth(request)) {
			logmessage += " Auth: Success";
			// Serial.println(logmessage);
			request->send(200, "text/plain", listFiles(true));
		} else {
			logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}
			});

	server->on("/flash_file", HTTP_GET, [](AsyncWebServerRequest * request) {

		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		if (checkUserWebAuth(request)) {

			logmessage += " Auth: Success";

			if (request->hasParam("name") && request->hasParam("area")) {

				const char *fileName = request->getParam("name")->value().c_str();
				const char *fileArea = request->getParam("area")->value().c_str();

				if (strcmp(fileArea, "rom") == 0) {
					Serial.println("Flash to rom");
					flash_rom(fileName);
				}
				else if (strcmp(fileArea, "ram") == 0){
					Serial.println("Flash to ram");
					flash_ram(fileName);
				}
			}
			// Serial.println(logmessage);

			// request->send(200, "text/html", checkLinkStatus());
			request->send(200, "text/html", "OK");
		} else {
			logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}
	});
	server->on("/checkLink", HTTP_GET, [](AsyncWebServerRequest * request)
			{
		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		if (checkUserWebAuth(request)) {
			logmessage += " Auth: Success";
			// Serial.println(logmessage);
			request->send(200, "text/html", checkLinkStatus());
		} else {
			logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}
	});

	server->on("/file", HTTP_GET, [](AsyncWebServerRequest * request) {
		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		if (checkUserWebAuth(request)) {
			logmessage += " Auth: Success";
			// Serial.println(logmessage);

			if (request->hasParam("name") && request->hasParam("action")) {
				const char *fileName = request->getParam("name")->value().c_str();
				const char *fileAction = request->getParam("action")->value().c_str();

				logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=/" + String(fileName) + "&action=" + String(fileAction);

				if (!SPIFFS.exists(fileName)) {
					// Serial.println(logmessage + " ERROR: file does not exist");
					request->send(400, "text/plain", "ERROR: file does not exist");
				} else {
					//Serial.println(logmessage + " file exists");
					if (strcmp(fileAction, "download") == 0) {
						logmessage += " downloaded";
						request->send(SPIFFS, fileName, "application/octet-stream");
					} else if (strcmp(fileAction, "delete") == 0) {
						logmessage += " deleted";
						SPIFFS.remove(fileName);
						request->send(200, "text/plain", "Deleted File: " + String(fileName));
					} else {
						logmessage += " ERROR: invalid action param supplied";
						request->send(400, "text/plain", "ERROR: invalid action param supplied");
					}
					//Serial.println(logmessage);
				}
			} else {
				request->send(400, "text/plain", "ERROR: name and action params required");
			}
		} else {
			logmessage += " Auth: Failed";
			// Serial.println(logmessage);
			return request->requestAuthentication();
		}
	});
}

void notFound(AsyncWebServerRequest *request) {
	String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
	// Serial.println(logmessage);
	request->send(404, "text/plain", "Not found");
}

// used by server.on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool checkUserWebAuth(AsyncWebServerRequest * request) {
	bool isAuthenticated = false;

	if (request->authenticate(httpuser.c_str(), httppassword.c_str())) {
		// Serial.println("is authenticated via username and password");
		isAuthenticated = true;
	}
	return isAuthenticated;
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	// make sure authenticated before allowing upload
	if (checkUserWebAuth(request)) {
		String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
		// Serial.println(logmessage);

		if (!index) {
			logmessage = "Upload Start: " + String(filename);
			// open the file on first call and store the file handle in the request object
			request->_tempFile = SPIFFS.open("/" + filename, "w");
			// Serial.println(logmessage);
		}

		if (len) {
			// stream the incoming chunk to the opened file
			request->_tempFile.write(data, len);
			logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
			// Serial.println(logmessage);
		}

		if (final) {
			logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
			// close the file handle as the upload is now done
			request->_tempFile.close();
			// Serial.println(logmessage);
			request->redirect("/");
		}
	} else {
		// Serial.println("Auth: Failed");
		return request->requestAuthentication();
	}
}

String checkLinkStatus(void) {
	uplink = zdiStatus();
	if (uplink) {
		return "<div style=\"background-color: green; color: white; padding: 10px; text-align: center;\">Online</div>";
	}
	else {
		return "<div style=\"background-color: red; color: white; padding: 10px; text-align: center;\">Offline</div>";
	}
}
