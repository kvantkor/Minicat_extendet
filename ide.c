#include "kernel.h"
#include "ide.h"
#include "vga.h"

// Порты контроллера IDE
#define IDE_DATA        0x1F0
#define IDE_SECTOR_CNT  0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_SEL   0x1F6
#define IDE_COMMAND     0x1F7
#define IDE_STATUS      0x1F7

static void ide_delay() {
    // 4 чтения порта статуса дают задержку ~400нс
    inb(IDE_STATUS); inb(IDE_STATUS);
    inb(IDE_STATUS); inb(IDE_STATUS);
}

int ide_wait_ready(uint8_t check_drq) {
    uint8_t status;
    while (1) {
        status = inb(IDE_STATUS);
        if (status == 0xFF) return -1; // Контроллера нет
        if (status & 0x01)  return -2; // Ошибка диска (ERR)
        if (!(status & 0x80)) {        // BSY сброшен
            if (!check_drq) return 0;
            if (status & 0x08) return 0; // DRQ установлен
        }
    }
}

void ide_read_sector(uint32_t lba, uint8_t* buffer) {
	if (ide_wait_ready(0) < 0) return;
	
    // Выбор диска (Master) и установка старших 4 бит LBA
    outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ide_delay();
    
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
    
    ide_delay();
    
    uint8_t status = inb(IDE_STATUS);
    if (status & 0x01) {
        vga_puts("IDE Error: Read failed!\n");
    }
}

void ide_write_sector(uint32_t lba, uint8_t* buffer) {
    if(ide_wait_ready(0) < 0) return;
    
    outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ide_delay();
    
    outb(IDE_SECTOR_CNT, 1);
    outb(IDE_LBA_LOW,  (uint8_t)lba);
    outb(IDE_LBA_MID,  (uint8_t)(lba >> 8));
    outb(IDE_LBA_HIGH, (uint8_t)(lba >> 16));
    
    outb(IDE_COMMAND, 0x30); // 1. Даем команду на запись

    if(ide_wait_ready(1) < 0) return; // 2. Ждем готовности принять данные (DRQ)

    // 3. Пишем 512 байт (один раз!)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(IDE_DATA, ptr[i]);
    }
    
    // 4. Ждем, пока диск переварит данные
    ide_delay();
    ide_wait_ready(0);

    // 5. КРИТИЧНО: Сброс кэша, чтобы данные не пропали при выключении
    outb(IDE_COMMAND, 0xE7); 
    ide_wait_ready(0);
}
