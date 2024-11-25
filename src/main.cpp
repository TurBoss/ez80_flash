
#include <stdio.h>
#include <string.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoOTA.h>
#include <esp32_io.h>
// #include <WiFiManager.h>


//#include "zdi.h"
//#include "cpu.h"
//#include "eZ80F92.h"
#include "config.h"
#include "webserver.h"
#include "webpages.h"
#include "utils.h"
#include "tool.h"


#define LEDFLASHFASTMS      75
#define LEDFLASHSTEADYMS    500
#define LEDFLASHWAIT        300

#define WAITHOLDBUTTONMS    2000
#define WAITRESETMS         10000


#define RGB_LED				21
#define USE_LITTLEFS		true


int bin_dest = 0;
int menu_page = 0;
int ledpins[] = {3, 4, 5};
int ledpins_onstate[] = {0, 0, 0};

bool textComplete = false;
bool cpuOnline = false;
bool needMenu = true;


AsyncWebServer *server;               // initialise webserver
//String inputText;



char zdi_msg_up[] = "ZDI up - id 0x%04X, revision 0x%02X\r\n\0";
char zdi_msg_down[] = "ZDI down - check cabling\r\n\0";

int porthttp = 80;

String ssid = "TP-LINK";
String wifipassword = "pladur13lol";

void setupLedPins(void);
void ledsOff(void);
void ledsOn(int red, int green, int blue);
void ledsFlash(int red, int green, int blue);
void ledsErrorFlash();
void ledsWaitFlash(int32_t ms);

void printMenus(uint8_t menu);

void printDestMenu(void);
void printFileMenu(void);


void setup() {

	// Disable the watchdog timers
	disableCore0WDT();

	delay(500);

	esp_task_wdt_init(30, false); // in case WDT cannot be removed

	// Serial
	Serial.begin(115200);

//	inputText.reserve(32);

	// setup LED pins
	setupLedPins();

	Serial.println("\r\nConnecting to Wifi: ");
	WiFi.begin(ssid.c_str(), wifipassword.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("\r\nMounting SPIFFS ...");
	if (!SPIFFS.begin(true)) {
		// if you have not used SPIFFS before on a ESP32, it will show this error.
		// after a reboot SPIFFS will be configured and will happily work.
		Serial.println("ERROR: Cannot mount SPIFFS, Rebooting");
		while (1){
			delay(100);
		}
		// rebootESP("ERROR: Cannot mount SPIFFS, Rebooting");
	}

	Serial.print("SPIFFS Free: "); Serial.println(humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
	Serial.print("SPIFFS Used: "); Serial.println(humanReadableSize(SPIFFS.usedBytes()));
	Serial.print("SPIFFS Total: "); Serial.println(humanReadableSize(SPIFFS.totalBytes()));

	Serial.println(listFiles(false));

	Serial.println("\r\n\r\nNetwork Configuration:");
	Serial.println("----------------------");
	Serial.print("         SSID: "); Serial.println(WiFi.SSID());
	Serial.print("  Wifi Status: "); Serial.println(WiFi.status());
	Serial.print("Wifi Strength: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
	Serial.print("          MAC: "); Serial.println(WiFi.macAddress());
	Serial.print("           IP: "); Serial.println(WiFi.localIP());
	Serial.print("       Subnet: "); Serial.println(WiFi.subnetMask());
	Serial.print("      Gateway: "); Serial.println(WiFi.gatewayIP());
	Serial.print("        DNS 1: "); Serial.println(WiFi.dnsIP(0));
	Serial.print("        DNS 2: "); Serial.println(WiFi.dnsIP(1));
	Serial.print("        DNS 3: "); Serial.println(WiFi.dnsIP(2));
	Serial.println("\0");

	ArduinoOTA
	.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else {  // U_SPIFFS
			type = "filesystem";
		}

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.println("Start updating " + type);
	})
	.onEnd([]() {
		Serial.println("\nEnd");
	})
	.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	})
	.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
			Serial.println("Auth Failed");
		} else if (error == OTA_BEGIN_ERROR) {
			Serial.println("Begin Failed");
		} else if (error == OTA_CONNECT_ERROR) {
			Serial.println("Connect Failed");
		} else if (error == OTA_RECEIVE_ERROR) {
			Serial.println("Receive Failed");
		} else if (error == OTA_END_ERROR) {
			Serial.println("End Failed");
		}
	});

	ArduinoOTA.begin();

	// configure web server
	Serial.println("Configuring Webserver ...");
	server = new AsyncWebServer(porthttp);
	configureWebServer(server);

	// startup web server
	Serial.println("Starting Webserver ...");

	server->begin();

	initZDI();

	// init_ez80();


	Serial.println("#############################");
	Serial.println("# ez80 Flash Utility v0.0.1 #");
	Serial.println("#############################");
}

void loop() {

// 	cpuOnline = zdiStatus();

//	if (cpuOnline){
//		if (needMenu) {
//			printMenus(1);
//			needMenu = false;
//		}
//
//
//		while (Serial.available()) {
//
//			char inChar = (char)Serial.read();
//
//			Serial.print(inChar);
//
//			if ((inChar == '\n') or (inChar == '\r')){
//				inputText += '\0';
//				textComplete = true;
//			}
//			else {
//				inputText += inChar;
//			}
//		}
//
//		if (textComplete) {
//
//			textComplete = false;
//			Serial.print("\r\n");
//
//			Serial.println(inputText);
//
//			if (menu_page == 1){
//				if ((inputText.toInt() == 1) or (inputText == "ram") or (inputText == "RAM")) {
//					bin_dest = 1;
//					menu_page = 2;
//				}
//				else if ((inputText.toInt() == 2) or (inputText == "rom") or (inputText == "ROM")) {
//					bin_dest = 2;
//					menu_page = 2;
//				}
//				else {
//					Serial.println("not an option");
//				}
//				printMenus(menu_page);
//			}
//			else if (menu_page == 2){
//
//				if (inputText.toInt() == 0){
//					menu_page = 1 ;
//					printMenus(menu_page);
//				}
//				else {
//
//					if(!SPIFFS.begin(true)){
//						Serial.println("An Error has occurred while mounting SPIFFS");
//						while(1) ledsFlash(0xff, 0x00, 0x00);
//					}
//
//					File root = SPIFFS.open("/");
//
//					File bin_file = root.openNextFile();
//
//					int i = 1;
//					const char *selectedFile;
//					const char *listedFile;
//
//					while(bin_file){
//						listedFile = bin_file.name();
//						if (strcmp(listedFile, "flash.bin")) {
//							if ((i == inputText.toInt()) or  (inputText == listedFile)) {
//								selectedFile = listedFile;
//								Serial.printf("%d %s\r\n\0", i, selectedFile);
//
//								break;
//							}
//							i ++;
//						}
//						bin_file = root.openNextFile();
//					}
//					if (bin_file){
//						Serial.print("Destination = ");
//
//						if (bin_dest == 1){
//							Serial.println("RAM");
//							flash_ram(selectedFile);
//						}
//						else if (bin_dest == 2) {
//							Serial.println("ROM");
//							flash_rom(selectedFile);
//						}
//					}
//					else{
//						ledsFlash(0xff, 0x00, 0x00);
//						Serial.println("no file.");
//					}
//					inputText = "";
//					needMenu = true;
//				}
//			}
//			inputText = "";
//		}
//	}
//	else {
//		if(needMenu) {
//			printMenus(1);
//			needMenu = false;
//		}
//
//
//		// reboot if we've told it to reboot
//		//	if (EspReboot) {
//		//		rebootESP("Web Admin Initiated Reboot");
//		//	}
//
//	}


// 	ArduinoOTA.handle();

}
void setupLedPins(void) {
	for(int n = 0; n < (sizeof(ledpins) / sizeof(int)); n++)
		directModeOutput(ledpins[n]);
}

void ledsOff(void) {

	neopixelWrite(RGB_LED, 0, 0, 0);
	for(int n = 0; n < (sizeof(ledpins) / sizeof(int)); n++)
		if(ledpins_onstate[n]) directWriteLow(ledpins[n]);
		else directWriteHigh(ledpins[n]);
}

void ledsOn(int red, int green, int blue) {

	neopixelWrite(RGB_LED, red, green, blue);
	for(int n = 0; n < (sizeof(ledpins) / sizeof(int)); n++)
		if(ledpins_onstate[n]) directWriteHigh(ledpins[n]);
		else directWriteLow(ledpins[n]);
}

void ledsFlash(int red, int green, int blue) {
	ledsOff();
	delay(LEDFLASHFASTMS);
	ledsOn(red, green, blue);
	delay(LEDFLASHFASTMS);
}

void ledsErrorFlash() {
	int n;
	for(n = 0; n < 4; n++) ledsFlash(0xff, 0x00, 0x00);
	delay(LEDFLASHWAIT);
}

void ledsWaitFlash(int32_t ms) {
	while(ms > 0) {
		ledsOff();
		delay(LEDFLASHSTEADYMS);
		ledsOn(0x00, 0xff, 0x00);
		delay(LEDFLASHSTEADYMS);
		ms -= 2*LEDFLASHSTEADYMS;
	}
}


void printFileMenu(void) {
	menu_page = 2;

	Serial.printf("\r\n#################################\r\n\0");
	Serial.printf(zdi_msg_up, getProductid(), getRevision());
	Serial.printf("#################################\r\n\0");


	File root = SPIFFS.open("/");

	File bin_file = root.openNextFile();

	delay(100);

	if (bin_dest == 1){
		Serial.println("Input number or file to flash on RAM:");
	}
	if (bin_dest == 2){
		Serial.println("Input number or file to flash on ROM:");
	}

	Serial.print("\r\n");

	int i = 1;
	while(bin_file){
		if (strcmp(bin_file.name(), "flash.bin")) {
			Serial.printf("\t%d - %s\r\n", i, bin_file.name());
			i ++;
		}
		bin_file = root.openNextFile();
	}

	Serial.println("\t0 - back");

	Serial.println("");
	Serial.print("> ");

}
void printDestMenu(void) {
	menu_page = 1;
	Serial.println("Upload bin to:");
	Serial.println("\t1 - RAM 0x40000");
	Serial.println("\t2 - ROM 0x00000");
	Serial.println("");
	Serial.print("> ");

}

void printMenus(uint8_t menu) {

	if (menu == 1){
		printDestMenu();
		menu_page = 1;
	}
	else if (menu == 2) {
		printFileMenu();
		menu_page = 2;
	}


}
