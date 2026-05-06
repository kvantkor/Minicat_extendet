#include "mboot_info.h"
#include <stdint.h>

/*
 * это файл парсера структуры multiboot
 * функция парсинга структуры
*/

uint32_t RAM_TOTAL_SIZE = 0;
uint32_t MMAP_PTR = 0;
uint32_t MMAP_LEN = 0;
char*    BOOT_CMDLINE = 0;
uint32_t KERNEL_END_ADDR = 0;

// функция парсинга структуры
void unpack_multiboot(uint32_t magic, uint32_t addr) {
    if (magic != 0x2BADB002) return; // Не мультибут

    multiboot_info_t* mbi = (multiboot_info_t*)addr;
    extern uint32_t end; // Ссылка на linker script
    KERNEL_END_ADDR = (uint32_t)&end;

    // 1. Память (базовая проверка)
    if (mbi->flags & 0x01) {
        RAM_TOTAL_SIZE = (mbi->mem_upper + 1024) * 1024;
    }

    // 2. Карта памяти (КРИТИЧНО для железа)
    if (mbi->flags & 0x40) {
        MMAP_PTR = mbi->mmap_addr;
        MMAP_LEN = mbi->mmap_length;
    }

    // 3. Параметры загрузки (если есть)
    if (mbi->flags & 0x04) {
        BOOT_CMDLINE = (char*)mbi->cmdline;
    }
}
