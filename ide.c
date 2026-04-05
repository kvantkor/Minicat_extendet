#include "kernel.h"
#include "ide.h"

// Порты контроллера IDE
#define IDE_DATA        0x1F0
#define IDE_SECTOR_CNT  0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_SEL   0x1F6
#define IDE_COMMAND     0x1F7
#define IDE_STATUS      0x1F7

void ide_read_sector(uint32_t lba, uint8_t* buffer) {
    // Выбор диска (Master) и установка старших 4 бит LBA
    outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Количество секторов (1)
    outb(IDE_SECTOR_CNT, 1);
    
    // Остальные биты LBA
    outb(IDE_LBA_LOW,  (uint8_t)lba);
    outb(IDE_LBA_MID,  (uint8_t)(lba >> 8));
    outb(IDE_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // Команда чтения (0x20)
    outb(IDE_COMMAND, 0x20);

    // Ждем, пока диск не сбросит бит BSY (Busy) и не поставит DRQ (Data Request)
    while ((inb(IDE_STATUS) & 0x88) != 0x08);

    // Читаем 512 байт (256 слов по 16 бит)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(IDE_DATA);
    }
}
