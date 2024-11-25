#include <stdint.h>


void initZDI(void);

void init_ez80(void);

void ZDI_upload(uint32_t address, uint8_t *buffer, uint32_t size, bool report);
uint32_t ZDI_upload(uint32_t address, const char *name, bool report);

uint32_t getfileCRC(const char *name);
uint32_t getZDImemoryCRC(uint32_t address, uint32_t size);

void flash_ram(const char *filename);
void flash_rom(const char *filename);

bool zdiStatus(void);
void resetCPU(void);
unsigned char *getProductid(void);
unsigned char *getRevision(void);
