#include "kernel.h"
#include "ide.h"
#include "vga.h"
#include <stdint.h>

volatile uint8_t ide_irq_fired = 0;

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
        status = inb(0x3F6);
        if (status == 0xFF) return -1; // Контроллера нет
        if (status & 0x01)  return -2; // Ошибка диска (ERR)
        if (!(status & 0x80)) {        // BSY сброшен
            if (!check_drq) return 0;
            if (status & 0x08) return 0; // DRQ установлен
        }
    }
}

void ide_read_sector(uint32_t lba, uint8_t* buffer) {
    ide_wait_ready(0); // Ждем через 0x3F6 (исправь функцию выше)
    ide_irq_fired = 0;

    outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(IDE_SECTOR_CNT, 1);
    outb(IDE_LBA_LOW,  (uint8_t)lba);
    outb(IDE_LBA_MID,  (uint8_t)(lba >> 8));
    outb(IDE_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(IDE_COMMAND, 0x20);

    // Даем диску миг, чтобы выставить BSY
    ide_delay(); 

    // Ждем прерывания. Если за 2 млн циклов не пришло — проверяем DRQ вручную
    int timeout = 2000000;
    while (!ide_irq_fired && --timeout > 0) {
        // Если диск вдруг сбросил BSY и поставил DRQ, а прерывания нет (глюк эмулятора)
        if (!(inb(0x3F6) & 0x80) && (inb(0x3F6) & 0x08)) break; 
    }

    // В любом случае читаем статус один раз из 0x1F7, чтобы подтвердить IRQ
    inb(0x1F7); 

    // Читаем данные
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) ptr[i] = inw(IDE_DATA);

    // Снова читаем статус для закрытия транзакции
    inb(0x1F7);
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
