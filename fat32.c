#include "fat32.h"
#include "ide.h"
#include "vga.h"
#include "shell.h"

// Глобальные переменные для работы с ФС
static uint32_t data_start_sector;
static uint32_t reserved_sectors;
static uint8_t  sectors_per_cluster;
static uint32_t sectors_per_fat;
static uint32_t root_cluster;
static uint32_t last_alloc_cluster = 2; // Храним последний выданный кластер
static uint32_t partition_lba_start = 0; // Смещение раздела

uint32_t cluster_to_lba(uint32_t cluster);
void fat32_set_cluster_value(uint32_t cluster, uint32_t value);
uint32_t fat32_get_next_cluster(uint32_t current_cluster);

void fat32_init() {
    uint8_t buf[512];
    
    ide_read_sector(0, buf);
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        // Проверяем первый раздел (смещение 446)
        // Тип 0x0B или 0x0C — это FAT32
        if (buf[446 + 4] == 0x0B || buf[446 + 4] == 0x0C) {
            partition_lba_start = *(uint32_t*)&buf[446 + 8];
        }
    }
	
	ide_read_sector(partition_lba_start, buf);
	
    // Выводим сигнатуру (должна быть 0x55 0xAA в конце)
    vga_puts("Sector 0 signature: ");
    vga_put_int(buf[510]);
    vga_putc(' ');
    vga_put_int(buf[511]);
    vga_puts("\n");

    // Выводим байты файловой системы (должны быть "FAT32   ")
    vga_puts("FS Type: ");
    for(int i=0; i<8; i++) vga_putc(buf[82+i]); 
    vga_puts("\n");

    // Парсим BPB (согласно спецификации FAT32)
    reserved_sectors    = *(uint16_t*)&buf[14];
    uint8_t fat_count   = buf[16];
    sectors_per_cluster = buf[13];
    sectors_per_fat     = *(uint32_t*)&buf[36];
    root_cluster        = *(uint32_t*)&buf[44];

    // Вычисляем начало области данных (сектор, где лежит кластер #2)
    data_start_sector = partition_lba_start + reserved_sectors + (fat_count * sectors_per_fat);
    
    vga_puts("FAT32 Initialized. Root Cluster: ");
    vga_put_int(root_cluster);
    vga_puts("\n");
}

void fat32_free_chain(uint32_t cluster) {
	uint8_t zero_buf[512];
	
	for(int i =0; i<512; i++) { zero_buf[i] = 0; }
	
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = fat32_get_next_cluster(cluster);
        
        // 1. Стираем данные в самом кластере на диске
        uint32_t lba = cluster_to_lba(cluster);
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_write_sector(lba + s, zero_buf);
        }

        // 2. Освобождаем кластер в таблице FAT
        fat32_set_cluster_value(cluster, 0);
        
        cluster = next;
	}
}

// Вспомогательная функция: перевод номера кластера в LBA сектор
uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_sector + (cluster - 2) * sectors_per_cluster;
}

// Получение следующего кластера из таблицы FAT
uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    uint8_t fat_buf[512];
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = partition_lba_start + reserved_sectors + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    ide_read_sector(fat_sector, fat_buf);
    
    uint32_t next = (*(uint32_t*)&fat_buf[ent_offset]) & 0x0FFFFFFF;
    return (next >= 0x0FFFFFF8) ? 0 : next; 
}

void fat32_list_root() {
    uint8_t buf[512];
    uint32_t current_cluster = root_cluster;

	vga_puts("Data start sector: "); vga_put_int(data_start_sector);
    vga_puts("\nSectors per cluster: "); vga_put_int(sectors_per_cluster);
    vga_puts("\nRoot cluster: "); vga_put_int(root_cluster);
    vga_puts("\n");
    
    uint32_t lba = cluster_to_lba(root_cluster);
    vga_puts("Root LBA: "); vga_put_int(lba);
    vga_puts("\n");
    
    // Читаем самый первый сектор корня
    ide_read_sector(lba, buf);
    vga_puts("First byte of root: "); vga_put_int(buf[0]);
    vga_puts("\n");

    vga_puts("Type     Name        Size\n");
    vga_puts("---------------------------\n");

    // Идем по цепочке кластеров директории
    while (current_cluster != 0) {
        uint32_t lba = cluster_to_lba(current_cluster);
        
        // Читаем все сектора внутри одного кластера
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, buf);
            fat32_entry_t* entry = (fat32_entry_t*)buf;

            for (int i = 0; i < 16; i++) { // 16 записей по 32 байта в секторе
                if (entry[i].name[0] == 0x00) return;   // Конец списка
                if (entry[i].name[0] == 0xE5) continue; // Удален
                if (entry[i].attr == 0x0F) continue;    // LFN

                // Вывод типа
                if (entry[i].attr & 0x10) vga_puts("<DIR>    ");
                else vga_puts("<FILE>   ");

                // Вывод имени (8.3)
                for (int j = 0; j < 8; j++) 
                    if (entry[i].name[j] != ' ') vga_putc(entry[i].name[j]);
                
                if (!(entry[i].attr & 0x10)) {
                    vga_putc('.');
                    for (int j = 0; j < 3; j++) 
                        if (entry[i].ext[j] != ' ') vga_putc(entry[i].ext[j]);
                }

                vga_puts("  -  ");
                vga_put_int(entry[i].size);
                vga_puts(" bytes\n");
            }
        }
        // Переходим к следующему кластеру, если директория большая
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

// Сравнивает обычную строку "FILE.TXT" с записью FAT "FILE    TXT"
static int fat32_name_match(const char* input, uint8_t* entry_name, uint8_t* entry_ext) {
    char disk_name[13]; // 8 + 3 + точка + ноль
    int p = 0;

    // Собираем имя с диска в строку "NAME.EXT"
    for (int i = 0; i < 8 && entry_name[i] != ' '; i++) disk_name[p++] = entry_name[i];
    if (entry_ext[0] != ' ') {
        disk_name[p++] = '.';
        for (int i = 0; i < 3 && entry_ext[i] != ' '; i++) disk_name[p++] = entry_ext[i];
    }
    disk_name[p] = '\0';

    // Сравниваем input и disk_name БЕЗ учета регистра
    const char *a = input;
    const char *b = disk_name;

    while (*a && *b) {
        char ca = *a;
        char cb = *b;

        if (ca >= 'a' && ca <= 'z') ca -= 32; // Приводим ввод к UPPER
        if (cb >= 'a' && cb <= 'z') cb -= 32; // Приводим диск к UPPER

        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

uint32_t fat32_find_file(const char* filename) {
    uint8_t buf[512];
    uint32_t current_cluster = root_cluster;

    while (current_cluster != 0) {
        uint32_t lba = cluster_to_lba(current_cluster);

        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, buf);
            fat32_entry_t* entry = (fat32_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                if (entry[i].name[0] == 0x00) return 0; // Конец списка
                if (entry[i].name[0] == 0xE5 || (entry[i].attr & 0x10)) continue;

                // Если имя совпало, возвращаем номер кластера
                if (fat32_name_match(filename, entry[i].name, entry[i].ext)) {
                    return ((uint32_t)entry[i].cluster_high << 16) | entry[i].cluster_low;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return 0; // Не нашли
}

void fat32_read_file(uint32_t cluster, uint8_t* target_buffer) {
    uint32_t current_cluster = cluster;
    uint32_t offset = 0;

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(current_cluster);
        
        // Читаем весь кластер целиком
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, target_buffer + offset);
            offset += 512;
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

uint32_t fat32_find_free_cluster() {
    uint8_t fat_buf[512];
    
    uint32_t start_sector = (last_alloc_cluster * 4) / 512;
    
    for (uint32_t s = start_sector; s < sectors_per_fat; s++) {
        ide_read_sector(reserved_sectors + s, fat_buf);
        uint32_t* entries = (uint32_t*)fat_buf;
        
        for (int i = 0; i < 128; i++) {
            uint32_t current_cl = (s * 128) + i;
            if (current_cl < 2) continue;

            if ((entries[i] & 0x0FFFFFFF) == 0) {
                last_alloc_cluster = current_cl; // Запоминаем для следующего раза
                return current_cl;
            }
        }
    }
    
    // Если дошли до конца и не нашли — попробуем один раз с начала (на случай если место освободилось сзади)
    last_alloc_cluster = 2; 
    return 0; 
}

int fat32_delete_file(const char* filename) {
    // 1. Защита системных файлов
    if (_strcmp(filename, "KERNEL.BIN") == 0 || _strcmp(filename, "BOOT.ASM") == 0) {
        return -1; 
    }

    uint8_t buf[512];
    uint32_t current_cluster = root_cluster;

    while (current_cluster != 0) {
        uint32_t lba = cluster_to_lba(current_cluster);
        
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, buf);
            fat32_entry_t* entries = (fat32_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) return -1; // Файл не найден
                if (entries[i].name[0] == 0xE5) continue;  // Пропускаем уже удаленные

                if (fat32_name_match(filename, entries[i].name, entries[i].ext)) {
                    // 2. Получаем первый кластер файла
                    uint32_t first_cl = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                    
                    // 3. Освобождаем цепочку в таблице FAT
                    fat32_free_chain(first_cl);

                    // 4. Помечаем запись в директории как удаленную (0xE5)
                    entries[i].name[0] = 0xE5;
                    ide_write_sector(lba + s, buf);
                    
                    return 0; // Успех
                }
            }
        }
        // Переход к следующему кластеру директории (если список файлов длинный)
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return -1;
}

void format_name_to_83(const char* src, uint8_t* dest) {
    // Заполняем всё пробелами (ASCII 0x20)
    for (int i = 0; i < 11; i++) dest[i] = 0x20;

    int i = 0, j = 0;
    // Копируем имя (до точки или конца строки)
    while (src[i] && src[i] != '.' && j < 8) {
        char c = src[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // В UPPER
        dest[j++] = c;
    }

    // Ищем точку для расширения
    while (src[i] && src[i] != '.') i++;
    if (src[i] == '.') {
        i++;
        j = 8; // Смещение расширения в FAT entry
        while (src[i] && j < 11) {
            char c = src[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            dest[j++] = c;
        }
    }
}

int fat32_create_file(const char* filename) {
    uint8_t buf[512];
    uint8_t name83[11];
    format_name_to_83(filename, name83);

    uint32_t current_cluster = root_cluster;

    while (current_cluster != 0) {
        uint32_t lba = cluster_to_lba(current_cluster);
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, buf);
            fat32_entry_t* entries = (fat32_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                // Если слот свободен (0x00) или был удален (0xE5)
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    
                    // 1. Ищем свободный кластер для данных файла
                    uint32_t new_cl = fat32_find_free_cluster();
                    if (new_cl == 0) return -1; // Диск полон

                    // 2. Заполняем запись
                    // Вместо одного цикла на 11:
					for(int k = 0; k < 8; k++) entries[i].name[k] = name83[k];   // Первые 8 байт (имя)
					for(int k = 0; k < 3; k++) entries[i].ext[k] = name83[8+k]; // Следующие 3 байта (расширение)
                    entries[i].attr = 0x20; // Archive
                    entries[i].cluster_low = new_cl & 0xFFFF;
                    entries[i].cluster_high = (new_cl >> 16) & 0xFFFF;
                    entries[i].size = 0; // Пока пустой

                    // 3. Сохраняем сектор директории
                    ide_write_sector(lba + s, buf);

                    // 4. Помечаем кластер в FAT как занятый (EOF)
                    fat32_set_cluster_value(new_cl, 0x0FFFFFFF);
                    
                    vga_puts("File created successfully.\n");
                    return 0;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return -1;
}

void fat32_set_cluster_value(uint32_t cluster, uint32_t value) {
    uint8_t fat_buf[512];
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = reserved_sectors + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

	asm volatile("cli");
    ide_read_sector(fat_sector, fat_buf);
    *(uint32_t*)&fat_buf[ent_offset] = value & 0x0FFFFFFF;
    ide_write_sector(fat_sector, fat_buf);
	asm volatile("sti");
}

int fat32_write_file(const char* filename, uint8_t* data, uint32_t size) {
    uint32_t cluster = fat32_find_file(filename);
    if (cluster == 0) return -1; 

    uint32_t bytes_left = size;
    uint32_t current_cluster = cluster;
    uint32_t data_offset = 0;

    asm volatile("cli"); 
    
    while (bytes_left > 0) {
        uint32_t lba = cluster_to_lba(current_cluster);
		
        // 1. Записываем ВСЕ сектора кластера
        for (uint32_t s = 0; s < sectors_per_cluster && bytes_left > 0; s++) {
            uint32_t to_write = (bytes_left > 512) ? 512 : bytes_left;
            
            ide_write_sector(lba + s, data + data_offset);
        
            data_offset += to_write;
            bytes_left -= to_write;
        }

        // 2. Если данные еще остались, переходим к следующему кластеру
        if (bytes_left > 0) {
            uint32_t next = fat32_get_next_cluster(current_cluster);
            if (next == 0) { 
                next = fat32_find_free_cluster();
                if (next == 0) { asm volatile("sti"); return -2; } 
                fat32_set_cluster_value(current_cluster, next);
                fat32_set_cluster_value(next, 0x0FFFFFFF); 
            }
            current_cluster = next;
        }
    }

    // 3. Фиксируем новый размер в записи директории
    fat32_update_entry(filename, size, cluster);

    asm volatile("sti");
    return 0;
}

void fat32_update_entry(const char* filename, uint32_t new_size, uint32_t new_cluster) {
    uint8_t buf[512];
    uint32_t current_cluster = root_cluster;

    while (current_cluster != 0) {
        uint32_t lba = cluster_to_lba(current_cluster);
        for (uint8_t s = 0; s < sectors_per_cluster; s++) {
            ide_read_sector(lba + s, buf);
            fat32_entry_t* entries = (fat32_entry_t*)buf;

            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) return;
                if (fat32_name_match(filename, entries[i].name, entries[i].ext)) {
                    // Обновляем данные
                    entries[i].size = new_size;
                    entries[i].cluster_low = new_cluster & 0xFFFF;
                    entries[i].cluster_high = (new_cluster >> 16) & 0xFFFF;
                    
                    // Записываем сектор обратно на диск
                    ide_write_sector(lba + s, buf);
                    return;
                }
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}
