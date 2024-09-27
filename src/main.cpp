#include <stdio.h>
#include <string.h>

#include "zdi.h"
#include "cpu.h"
#include "eZ80F92.h"
#include <esp_task_wdt.h>
#include "esp32_io.h"
#include "SPIFFS.h"
#include <CRC32.h>

#define PAGESIZE            2048
#define USERLOAD            0x40000
#define BREAKPOINT          0x40020
#define FLASHLOAD           0x50000
#define MOSSIZE_ADDRESS     0x70000
#define MOSDONE             0x70003

#define LEDFLASHFASTMS      75
#define LEDFLASHSTEADYMS    500
#define LEDFLASHWAIT        300

#define WAITHOLDBUTTONMS    2000
#define WAITRESETMS         10000
#define WAITPROGRAMSECS     20

#define ZDI_TCKPIN			1
#define ZDI_TDIPIN			2

#define RGB_LED				21

int ledpins[] = {3, 4, 5};
int ledpins_onstate[] = {0, 0, 0};

bool textComplete = false;
bool cpuOnline = false;
bool needMenu = true;

String inputText;

CPU*                    cpu;
ZDI*                    zdi;


char zdi_msg_up[] = "ZDI up - id 0x%04X, revision 0x%02X\r\n\0";
char zdi_msg_down[] = "ZDI down - check cabling\r\n\0";
char wait_symbols[] = "\\|/-\\|/-\0";

void setupLedPins(void);
void ledsOff(void);
void ledsOn(int red, int green, int blue);
void ledsFlash(int red, int green, int blue);
void ledsErrorFlash();
void ledsWaitFlash(int32_t ms);

uint32_t getfileCRC(const char *name);
uint32_t getZDImemoryCRC(uint32_t address, uint32_t size);

void init_ez80(void);

void ZDI_upload(uint32_t address, uint8_t *buffer, uint32_t size, bool report);
uint32_t ZDI_upload(uint32_t address, const char *name, bool report);
uint32_t getZDImemoryCRC(uint32_t address, uint32_t size);
void flash_ram(const char *filename);

void flash_rom(const char *filename);

void printSerialMenu(void);
void printMenus(void);

void zdiStatus(void);



void setup() {

    // Disable the watchdog timers
    disableCore0WDT();

    delay(500);

    esp_task_wdt_init(30, false); // in case WDT cannot be removed

    // Serial
    Serial.begin(115200);

    inputText.reserve(32);

    // setup ZDI interface
    zdi = new ZDI(ZDI_TCKPIN, ZDI_TDIPIN);

    cpu = new CPU(zdi);

    // setup LED pins
    setupLedPins();


    // init spiffs
    if(!SPIFFS.begin(true)){
        Serial.println("An Error has occurred while mounting SPIFFS");
        while(1) ledsFlash(0xff, 0x00, 0x00);
    }

    neopixelWrite(RGB_LED, 0x00, 0x00, 0xff);
}

void loop() {

	zdiStatus();

	if (cpuOnline){


	    if(needMenu) {
	        printMenus();
	        needMenu = false;
	    }


		while (Serial.available()) {

			char inChar = (char)Serial.read();

			Serial.print(inChar);

			if ((inChar == '\n') or (inChar == '\r')){
				textComplete = true;
			}
			else {
				inputText += inChar;
			}
		}

		if (textComplete) {

			textComplete = false;
			Serial.print("\r\n");

			// Serial.print(inputText);

			if(!SPIFFS.begin(true)){
				Serial.println("An Error has occurred while mounting SPIFFS");
				while(1) ledsFlash(0xff, 0x00, 0x00);
			}

			File root = SPIFFS.open("/");

			File bin_file = root.openNextFile();

			int i = 1;
			const char *selectedFile;
			const char *listedFile;

			while(bin_file){
				listedFile = bin_file.name();
				if (strcmp(listedFile, "flash.bin")) {
					if ((i == inputText.toInt()) or  (inputText == listedFile)) {
						selectedFile = listedFile;
						Serial.printf("%d %s\r\n\0", i, selectedFile);

						break;
					}
					i ++;
				}
				bin_file = root.openNextFile();
			}
			if (bin_file){
				Serial.println("OK!");
				flash_rom(selectedFile);
				// flash_ram(selecteFile);
			}
			else{
		        ledsFlash(0xff, 0x00, 0x00);
				Serial.println("no file.");
			}

			inputText = "";
	        needMenu = true;
		}
	}
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


uint32_t getfileCRC(const char *name) {
	CRC32 crc;
    uint32_t size, retsize;
    uint8_t buffer[PAGESIZE];

    retsize = 0;

    FILE *file = fopen(name, "r");

    crc.restart();
    
    while (size = fread(buffer, 1, PAGESIZE, file)) {

        for(int n = 0; n < size; n++) {
        	crc.add(buffer[n]);
        }

    }

    fclose(file);
    return crc.calc();
}

void init_ez80(void) {

	Serial.println("SET Break");
    cpu->setBreak();

	Serial.println("reset");
    cpu->reset();


	Serial.println("SET Break");
    cpu->setBreak();

    Serial.println("Set ADL MODE");
    // Set ADL MODE
    cpu->setADLmode(true);

    Serial.println("Disable interrupts");
    // Disable interrupts
    cpu->instruction_di();  

    Serial.println("configure SPI");
    // configure SPI
    cpu->instruction_out (SPI_CTL, 0x04);

    Serial.println("configure default GPIO");
    // configure default GPIO
    cpu->instruction_out (PB_DDR, 0xff);
    cpu->instruction_out (PC_DDR, 0xff);
    cpu->instruction_out (PD_DDR, 0xff);
    
    cpu->instruction_out (PB_ALT1, 0x0);
    cpu->instruction_out (PC_ALT1, 0x0);
    cpu->instruction_out (PD_ALT1, 0x0);
    cpu->instruction_out (PB_ALT2, 0x0);
    cpu->instruction_out (PC_ALT2, 0x0);
    cpu->instruction_out (PD_ALT2, 0x0);

    cpu->instruction_out (TMR0_CTL, 0x0);
    cpu->instruction_out (TMR1_CTL, 0x0);
    cpu->instruction_out (TMR2_CTL, 0x0);
    cpu->instruction_out (TMR3_CTL, 0x0);
    cpu->instruction_out (TMR4_CTL, 0x0);
    cpu->instruction_out (TMR5_CTL, 0x0);

    cpu->instruction_out (UART0_IER, 0x0);
    cpu->instruction_out (UART1_IER, 0x0);

    cpu->instruction_out (I2C_CTL, 0x0);
    cpu->instruction_out (FLASH_IRQ, 0x0);

    cpu->instruction_out (SPI_CTL, 0x4);

    cpu->instruction_out (RTC_CTRL, 0x0);

    Serial.println("configure internal flash");
    // configure internal flash
    cpu->instruction_out (FLASH_ADDR_U,0x00);
    cpu->instruction_out (FLASH_CTRL,0b00101000);   // flash enabled, 1 wait state
    
    // configure internal RAM chip-select range
    Serial.println("configure internal RAM chip-select range");
    cpu->instruction_out (RAM_ADDR_U,0xb7);         // configure internal RAM chip-select range
    cpu->instruction_out (RAM_CTL,0b10000000);      // enable
    Serial.println("configure external RAM chip-select range");
    // configure external RAM chip-select range
    cpu->instruction_out (CS0_LBR,0x04);            // lower boundary
    cpu->instruction_out (CS0_UBR,0x0b);            // upper boundary
    cpu->instruction_out (CS0_BMC,0b00000001);      // 1 wait-state, ez80 mode
    cpu->instruction_out (CS0_CTL,0b00001000);      // memory chip select, cs0 enabled

    // configure external RAM chip-select range
    cpu->instruction_out (CS1_CTL,0x00);            // memory chip select, cs1 disabled
    Serial.println("configure external RAM chip-select range");
    // configure external RAM chip-select range
    cpu->instruction_out (CS2_CTL,0x00);            // memory chip select, cs2 disabled
    Serial.println("configure external RAM chip-select range");
    // configure external RAM chip-select range
    cpu->instruction_out (CS3_CTL,0x00);            // memory chip select, cs3 disabled

    Serial.println("set stack pointer");
    // set stack pointer
    cpu->sp(0x0BFFFF);

    Serial.println("set program counter");
    // set program counter
    cpu->pc(0x000000);
}

// Upload to ZDI memory from a buffer
void ZDI_upload(uint32_t address, uint8_t *buffer, uint32_t size, bool report) {
	int i = 0;

    while(size > PAGESIZE) {
        zdi->write_memory(address, PAGESIZE, buffer);
        address += PAGESIZE;
        buffer += PAGESIZE;
        size -= PAGESIZE;
        ledsFlash(0x00, 0x00, 0xff);
        if(report) {
        	if (i >= sizeof(wait_symbols)) {
        		i = 0;
        	}
        	Serial.print(wait_symbols[i]);
        	Serial.print("\r");
        	i++;
        }
    }
    zdi->write_memory(address, size, buffer);

    if(report) {
    	if (i >= sizeof(wait_symbols)) {
    		i = 0;
    	}
    	Serial.print(wait_symbols[i]);
    	Serial.print("\r");
    	i++;
    }

    ledsFlash(0x00, 0x00, 0xff);
}

// Upload to ZDI memory from a file
uint32_t ZDI_upload(uint32_t address, const char *name, bool report) {

    uint32_t size, retsize;
    uint8_t buffer[PAGESIZE];
    uint8_t i = 0;

    FILE *file = fopen(name, "r");
    
    retsize = 0;


    while(size = fread(buffer, 1, PAGESIZE, file)) {
        zdi->write_memory(address, size, buffer);
        address += size;
        retsize += size;

        ledsFlash(0x00, 0x00, 0xff);

        if(report){
        	if (i >= sizeof(wait_symbols)) {
        		i = 0;
        	}
        	Serial.print(wait_symbols[i]);
        	Serial.print("\r");
        	i++;
        }
    }

    fclose(file);

    return retsize;
}

uint32_t getZDImemoryCRC(uint32_t address, uint32_t size) {
    CRC32 crc;

    uint32_t crcbytes = PAGESIZE;
    uint8_t buffer[PAGESIZE];

    crc.restart();

    cpu->setBreak();
    while(size) {
        if(size >= PAGESIZE) {
            zdi->read_memory(address, PAGESIZE, buffer);
            address += PAGESIZE;
            size -= PAGESIZE;
        }
        else {
            zdi->read_memory(address, size, buffer);
            crcbytes = size;
            address += size;
            size = 0;
        }
        for(int n = 0; n < crcbytes; n++) crc.add(buffer[n]);
    }
    return crc.calc();
}




void flash_ram(const char *filename) {

    char filepath[64] = "/spiffs/\0";

    strcpy(filepath, filename);

    int i = 0;

    uint32_t size;
    uint32_t read_moscrc;
    uint32_t expected_moscrc;


    init_ez80();

    // Upload the MOS payload to ZDI memory first
    Serial.printf("\r\nUploading %s ...\r\n\0", filepath);

	Serial.println("1 checking file CRC32...");

	expected_moscrc = getfileCRC(filepath);

	Serial.println(expected_moscrc);

    size = ZDI_upload(USERLOAD, filepath, true);

    read_moscrc = getZDImemoryCRC(USERLOAD, size);

    Serial.println("done.");
    // Serial.println(read_moscrc);

    if(read_moscrc == expected_moscrc) {
    	Serial.println("CRC32 OK");
    }
    else {

    	Serial.println("CRC32 ERROR");

        ledsFlash(0xff, 0x00, 0x00);
    	Serial.print("expected: ");
		Serial.println(expected_moscrc);
    	Serial.print("read: ");
    	Serial.println(read_moscrc);

        return;
    }

//    zdi->write_memory_24bit(MOSSIZE_ADDRESS, size);
//
//    ledsFlash(0x00, 0x00, 0xff);
//
//    // Upload the flash tool to ZDI memory next, so it can pick up the payload
//    Serial.println("Uploading flash tool...");
//    ledsFlash(0x00, 0x00, 0xff);
//    ZDI_upload(USERLOAD, "/spiffs/flash.bin", true);
//    ledsFlash(0x00, 0x00, 0xff);
//    Serial.println("done.");
//
//    // Run the CPU from the flash tool address
//    Serial.printf("\r\nFlashing %s ... \r\n\0", filename);
//
//    cpu->pc(USERLOAD);
//    cpu->setContinue(); // start flashloader, no feedback
//
//    // This is a CRITICAL wait and cannot be interrupted by ZDI
//    for(int n = 0; n < WAITPROGRAMSECS; n++) {
//        ledsFlash(0x00, 0x00, 0xff);
//        delay(1000);{
//		if (i >= sizeof(wait_symbols)) {
//        		i = 0;
//        	}
//        	Serial.print(wait_symbols[i]);
//        	Serial.print("\r");
//        	i++;
//        }
//    }
//    Serial.println("done.");

    // Final check
    cpu->setBreak();

    delay(100);


//    read_moscrc == getZDImemoryCRC(0, size);
//
//    if(read_moscrc == expected_moscrc) {
//    	Serial.println("CRC32 OK\r\n");
//    }
//    else {
//    	Serial.println("CRC32 ERROR\r\n");
//
//    	Serial.print("expected: ");
//		Serial.println(expected_moscrc);
//    	Serial.print("read: ");
//    	Serial.println(read_moscrc);
//
//        ledsFlash(0xff, 0x00, 0x00);
//        return;
//    }

    cpu->reset();

}

void flash_rom(const char *filename) {

    char filepath[256];

    uint32_t i;
    uint32_t size;
    uint32_t read_moscrc;
    uint32_t expected_moscrc;

    // Upload the MOS payload to ZDI memory first
    Serial.printf("\r\nUploading %s ...\r\n\0", filename);

    snprintf(filepath, sizeof(filepath), "/spiffs/%s\0", filename);

	expected_moscrc = getfileCRC(filepath);

    Serial.println("Init CPU.");

    init_ez80();

    Serial.println("ez80 initialized!");
    // delay(1000);

    size = ZDI_upload(FLASHLOAD, filepath, true);

    read_moscrc = getZDImemoryCRC(FLASHLOAD, size);

    Serial.println("done.");

    if(read_moscrc == expected_moscrc) {
    	Serial.println("CRC32 OK");
    }
    else {

    	Serial.println("CRC32 ERROR");

        ledsFlash(0xff, 0x00, 0x00);
    	Serial.print("expected: \0");
		Serial.println(expected_moscrc);
    	Serial.print("read: \0");
    	Serial.println(read_moscrc);

        return;
    }

    zdi->write_memory_24bit(MOSSIZE_ADDRESS, size);

    ledsFlash(0x00, 0x00, 0xff);
     
    // Upload the flash tool to ZDI memory next, so it can pick up the payload
    Serial.println("Uploading flash tool...");
    ledsFlash(0x00, 0x00, 0xff);
    ZDI_upload(USERLOAD, "/spiffs/flash.bin", true);
    ledsFlash(0x00, 0x00, 0xff);
    Serial.println("done.");

    // Run the CPU from the flash tool address
    Serial.printf("\r\nFlashing %s ... \r\n\0", filename);

    cpu->pc(USERLOAD);
    cpu->setContinue(); // start flashloader, no feedback  

    // This is a CRITICAL wait and cannot be interrupted by ZDI

    i = 0;

    for(int n = 0; n < WAITPROGRAMSECS; n++) { 
        ledsFlash(0x00, 0x00, 0xff);
        delay(1000);
		if (i >= sizeof(wait_symbols)) {
			i = 0;
		}
		Serial.print(wait_symbols[i]);
		Serial.print("\r");
		i++;
	}

    Serial.println("done.");
    
    // Final check
    cpu->setBreak();

    delay(100);


    read_moscrc == getZDImemoryCRC(0, size);

    if(read_moscrc == expected_moscrc) {
    	Serial.println("CRC32 OK\r\n");
    }
    else {
    	Serial.println("CRC32 ERROR\r\n");

    	Serial.print("expected: ");
		Serial.println(expected_moscrc);
    	Serial.print("read: ");
    	Serial.println(read_moscrc);

        ledsFlash(0xff, 0x00, 0x00);
        return;
    }

    cpu->reset();

}

void printSerialMenu(void) {

	Serial.printf("\r\n\r\n#################################\r\n\0");
	Serial.printf(zdi_msg_up, zdi->get_productid(), zdi->get_revision());
	Serial.printf("#################################\r\n\0");

	File root = SPIFFS.open("/");

	File bin_file = root.openNextFile();

	delay(1000);

	Serial.println("Input number or file to flash:");

	Serial.println("");

	int i = 1;
	while(bin_file){
		if (strcmp(bin_file.name(), "flash.bin")) {
			Serial.printf("\t%d - %s\r\n", i, bin_file.name());
			i ++;
		}
		bin_file = root.openNextFile();
	}

	Serial.println("");
	Serial.print("> ");

}


void printMenus(void) {
    printSerialMenu();
}

void zdiStatus(void) {
    while((zdi->get_productid() != 7)) {
        if(!needMenu) {
            needMenu = true;
        }
        ledsErrorFlash();
        ledsOff();
        Serial.printf(zdi_msg_down);
        cpuOnline = false;
        delay(500);
    }
    if(needMenu) {
        needMenu = false;
    	printSerialMenu();
    }

    cpuOnline = true;
}






