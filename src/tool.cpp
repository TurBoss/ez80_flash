
#include <stdio.h>
#include <string.h>

#include <CRC32.h>

#include "zdi.h"
#include "cpu.h"
#include "eZ80F92.h"


#define WAITPROGRAMSECS     10

#define PAGESIZE            2048
#define USERLOAD            0x40000
#define BREAKPOINT          0x40020
#define FLASHLOAD           0x50000
#define MOSSIZE_ADDRESS     0x70000
#define MOSDONE             0x70003

#define ZDI_TCKPIN			1
#define ZDI_TDIPIN			2

CPU*                    cpu;
ZDI*                    zdi;



char wait_symbols[] = "\\|/-\\|/-\0";

void initZDI(void) {
	// setup ZDI interface
	zdi = new ZDI(ZDI_TCKPIN, ZDI_TDIPIN);
	cpu = new CPU(zdi);
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

	// Serial.println("SET Break");
	cpu->setBreak();

	// Serial.println("reset");
	cpu->reset();


	// Serial.println("SET Break");
	cpu->setBreak();

	// Serial.println("Set ADL MODE");
	// Set ADL MODE
	cpu->setADLmode(true);

	// Serial.println("Disable interrupts");
	// Disable interrupts
	cpu->instruction_di();

	// Serial.println("configure SPI");
	// configure SPI
	cpu->instruction_out (SPI_CTL, 0x04);

	// Serial.println("configure default GPIO");
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

	// Serial.println("configure internal flash");
	// configure internal flash
	cpu->instruction_out (FLASH_ADDR_U,0x00);
	cpu->instruction_out (FLASH_CTRL,0b00101000);   // flash enabled, 1 wait state

	// configure internal RAM chip-select range
	// Serial.println("configure internal RAM chip-select range");
	cpu->instruction_out (RAM_ADDR_U,0xb7);         // configure internal RAM chip-select range
	cpu->instruction_out (RAM_CTL,0b10000000);      // enable
	//Serial.println("configure external RAM chip-select range");
	// configure external RAM chip-select range
	cpu->instruction_out (CS0_LBR,0x04);            // lower boundary
	cpu->instruction_out (CS0_UBR,0x0b);            // upper boundary
	cpu->instruction_out (CS0_BMC,0b00000001);      // 1 wait-state, ez80 mode
	cpu->instruction_out (CS0_CTL,0b00001000);      // memory chip select, cs0 enabled

	// configure external RAM chip-select range
	cpu->instruction_out (CS1_CTL,0x00);            // memory chip select, cs1 disabled
	//Serial.println("configure external RAM chip-select range");
	// configure external RAM chip-select range
	cpu->instruction_out (CS2_CTL,0x00);            // memory chip select, cs2 disabled
	//Serial.println("configure external RAM chip-select range");
	// configure external RAM chip-select range
	cpu->instruction_out (CS3_CTL,0x00);            // memory chip select, cs3 disabled

	//Serial.println("set stack pointer");
	// set stack pointer
	cpu->sp(0x0BFFFF);

	//Serial.println("set program counter");
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
		// ledsFlash(0x00, 0x00, 0xff);
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

	// ledsFlash(0x00, 0x00, 0xff);
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

		// ledsFlash(0x00, 0x00, 0xff);

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

	size = ZDI_upload(USERLOAD, filepath, true);

	read_moscrc = getZDImemoryCRC(USERLOAD, size);

	Serial.println("done.");
	// Serial.println(read_moscrc);

	if(read_moscrc == expected_moscrc) {
		Serial.println("CRC32 OK");
	}
	else {

		Serial.println("CRC32 ERROR");

		//ledsFlash(0xff, 0x00, 0x00);
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

	cpu->pc(USERLOAD);
	cpu->setContinue();

	// Final check
	//    cpu->setBreak();
	//
	//    delay(100);
	//
	//    cpu->pc(0x000000);



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

	// cpu->reset();
	Serial.println("Flash RAM OK!");

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

	Serial.println(filename);
	Serial.println(filepath);

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

		//ledsFlash(0xff, 0x00, 0x00);
		Serial.print("expected: \0");
		Serial.println(expected_moscrc);
		Serial.print("read: \0");
		Serial.println(read_moscrc);

		return;
	}

	zdi->write_memory_24bit(MOSSIZE_ADDRESS, size);

	//ledsFlash(0x00, 0x00, 0xff);

	// Upload the flash tool to ZDI memory next, so it can pick up the payload
	Serial.println("Uploading flash tool...");
	//ledsFlash(0x00, 0x00, 0xff);
	ZDI_upload(USERLOAD, "/spiffs/flash.bin", true);
	//ledsFlash(0x00, 0x00, 0xff);
	Serial.println("done.");

	// Run the CPU from the flash tool address
	Serial.printf("\r\nFlashing %s ... \r\n\0", filepath);

	cpu->pc(USERLOAD);
	cpu->setContinue(); // start flashloader, no feedback

	// This is a CRITICAL wait and cannot be interrupted by ZDI

	// Serial.println("~~~~~~~~~~~~~~~~");

	i = 0;
	for(int n = 0; n < WAITPROGRAMSECS; n++) {
		//ledsFlash(0x00, 0x00, 0xff);
		delay(1000);
		if (i > sizeof(wait_symbols)) {
			i = 0;
		}
		Serial.print(wait_symbols[i]);
		Serial.print("\r");
		i++;
	}

	Serial.println("\r\nDone.");

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

		//ledsFlash(0xff, 0x00, 0x00);
		return;
	}

	cpu->reset();

	Serial.println("Flash ROM OK!");

}

bool online;

bool zdiStatus(void) {
	if((zdi->get_productid() != 7)) {
		online = false;
	}
	else {
		online = true;
	}
	//setLinkStatus(cpuOnline);
	return online;
}

void resetCPU(void) {
	cpu->reset();
}

uint16_t getProductid(void) {
	return zdi->get_productid();
}
uint8_t getRevision(void) {
	return zdi->get_revision();
}



