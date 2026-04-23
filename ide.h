#ifndef IDE_H
#define IDE_H
#include <stdint.h>

void ide_read_sector(uint32_t lba, uint8_t* buffer);
void ide_write_sector(uint32_t lba, uint8_t *buffer);

extern volatile uint8_t ide_irq_fired;

#endif

