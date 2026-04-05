#include "fat32.h"
#include "ide.h"
#include "vga.h"

static uint32_t data_start_sector;

void fat32_init() {
    uint8_t buf[512];
    ide_read_sector(0, buf); // Читаем Boot Sector

    // Извлекаем важные данные из BPB
    uint16_t reserved_sectors = *(uint16_t*)&buf[14];
    uint8_t  fat_count = buf[16];
    uint32_t sectors_per_fat = *(uint32_t*)&buf[36];

    // Вычисляем начало области данных (где лежат файлы)
    // В FAT32 корневой каталог идет сразу за таблицами FAT
    data_start_sector = reserved_sectors + (fat_count * sectors_per_fat);
    
    vga_puts("FAT32: Data area starts at ");
    vga_put_int(data_start_sector);
    vga_puts("\n");
}

void fat32_list_root() {
    uint8_t buf[512];
    ide_read_sector(data_start_sector, buf); // Читаем первый сектор корневого каталога

    fat32_entry_t* entry = (fat32_entry_t*)buf;

    vga_puts("Type     Name        Size\n");
    vga_puts("---------------------------\n");

    for (int i = 0; i < 16; i++) { // В одном секторе 512/32 = 16 записей
        if (entry[i].name[0] == 0x00) break;   // Пустая запись - конец списка
        if (entry[i].name[0] == 0xE5) continue; // Удаленный файл
        if (entry[i].attr == 0x0F) continue;    // Длинное имя (LFN) - пока пропускаем

        // Выводим тип (DIR или FILE)
        if (entry[i].attr & 0x10) vga_puts("<DIR>    ");
        else vga_puts("<FILE>   ");

        // Выводим имя (8 символов)
        for (int j = 0; j < 8; j++) {
            if (entry[i].name[j] != ' ') vga_putc(entry[i].name[j]);
        }
        
        // Выводим расширение (3 символа)
        if (!(entry[i].attr & 0x10)) {
            vga_putc('.');
            for (int j = 0; j < 3; j++) vga_putc(entry[i].ext[j]);
        }

        vga_puts("    ");
        vga_put_int(entry[i].size);
        vga_puts(" bytes\n");
    }
}
