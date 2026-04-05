#include "pmm.h"
#include "vga.h"

#define BLOCKS_PER_BYTE 8

// Метка конца ядра из linker.ld
extern uint32_t end;

uint32_t free_blocks;
uint32_t total_blocks;
static uint8_t* pmm_bitmap;
static uint32_t bitmap_size;
static uint32_t next_free_addr = 0;
static uint32_t current_page_end = 0;

// Установка бита (занято)
static void pset(uint32_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

// Сброс бита (свободно)
static void punset(uint32_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Проверка бита (1 - занято, 0 - свободно)
static int ptest(uint32_t bit) {
    return pmm_bitmap[bit / 8] & (1 << (bit % 8));
}

void pmm_init(uint32_t mem_size) {
    total_blocks = mem_size / PAGE_SIZE;
    bitmap_size = total_blocks / BLOCKS_PER_BYTE;
	free_blocks = 0;
	
    // Битовую карту кладем сразу после ядра
    pmm_bitmap = (uint8_t*)&end;

    // 1. Сначала помечаем ВООБЩЕ ВСЁ как занятое (единицы)
    for (uint32_t i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // 2. Освобождаем блоки памяти. 
    // Для начала просто высвободим первые 64МБ (кроме первой страницы BIOS)
    // В идеале тут должен быть разбор карты памяти Multiboot
    for (uint32_t i = 1; i < (64 * 1024 * 1024) / PAGE_SIZE; i++) {
        punset(i);
        free_blocks++;
    }

    // 3. Помечаем страницы самого ядра и битовой карты как ЗАНЯТЫЕ
    // Чтобы PMM не выдал нам память, где лежит наш собственный код
	uint32_t kernel_pages = ((uint32_t)&end + bitmap_size) / PAGE_SIZE + 1;
    for (uint32_t i = 0; i < kernel_pages; i++) {
        if (!ptest(i)) continue; // Если уже занято (например, BIOS область), не трогаем
        pset(i);
        // Если мы помечаем страницу как занятую после того как она была свободной:
        free_blocks--; 
    }

    vga_puts("PMM: Bitmap placed at 0x");
    // Тут можно вывести адрес, если есть vga_put_hex
    vga_puts(". Memory managed.\n");
}

void* pmm_alloc_block() {
    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!ptest(i)) {
            pset(i);
			free_blocks--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return 0; // Память кончилась
}

void pmm_free_block(void* addr) {
    uint32_t bit = (uint32_t)addr / PAGE_SIZE;
    punset(bit);
}

void* kmalloc(uint32_t size) {
    // Выравниваем запрос по 4 байта
    size = (size + 3) & ~3;

    // Если это первый вызов или в текущей странице нет места
    if (next_free_addr == 0 || next_free_addr + size >= current_page_end) {
        void* new_page = pmm_alloc_block();
        if (!new_page) return 0; // Память кончилась

        next_free_addr = (uint32_t)new_page;
        current_page_end = next_free_addr + PAGE_SIZE;
    }

    void* res = (void*)next_free_addr;
    next_free_addr += size;
    return res;
}
