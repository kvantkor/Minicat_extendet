#include "pmm.h"
#include "vga.h"
#include "vmm.h"
#include "mboot_info.h"
#include <stddef.h>

#define BLOCKS_PER_BYTE 8

/*
 * это файл физического менеджера памяти
 * функция инициализации пмм
 * функция аллокатора физ. памяти
 * функция выделения нескольки блоков
 * функция перевода адреса в номер бита
 * функция привязки физ. адреса к виртуальному
 * функция дэмаппинга памяти
 * функция выделения памяти 
 * функция освобождения памяти
*/

// Метка конца ядра из linker.ld
extern uint32_t end;

uint32_t free_blocks;
uint32_t total_blocks;
static uint8_t* pmm_bitmap;
static uint32_t bitmap_size;
// static uint32_t next_free_addr = 0;
// static uint32_t current_page_end = 0;
static uint32_t next_virt_addr = 0x400000; // Начнем выделять выше первых 4МБ
static block_header_t* heap_start = NULL;

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

// инциализация пмм
void pmm_init() {
	free_blocks = 0;
	// 1. Расчёт блоков озу
    total_blocks = RAM_TOTAL_SIZE / PAGE_SIZE;
    bitmap_size = total_blocks / 8;
    
    pmm_bitmap = (uint8_t*)KERNEL_END_ADDR;
    //uint32_t bitmap_pages = (bitmap_size / PAGE_SIZE) + 1;

    // Сначала всё занято
    for (uint32_t i = 0; i < bitmap_size; i++) pmm_bitmap[i] = 0xFF;

    // 2. Идем по карте памяти
    multiboot_mmap_entry_t* mmap = (multiboot_mmap_entry_t*)MMAP_PTR;
    while((uint32_t)mmap < MMAP_PTR + MMAP_LEN) {
        if (mmap->type == 1) { // Если RAM доступна
            uint32_t addr = (uint32_t)mmap->addr;
            uint32_t end_addr = addr + (uint32_t)mmap->len;

            for (; addr < end_addr; addr += PAGE_SIZE) {
                // ЗАЩИТА: Не трогаем первые 1МБ и область ядра с битмапом
                uint32_t safe_zone = (uint32_t)pmm_bitmap + bitmap_size;
                if (addr < 0x100000) continue; 
                if (addr >= 0x100000 && addr < safe_zone) continue;

                punset(addr / PAGE_SIZE);
                free_blocks++;
            }
        }
        mmap = (multiboot_mmap_entry_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
}

//функция аллокатор физ. памяти
void* pmm_alloc_block() {
    for (uint32_t i = 1; i < total_blocks; i++) { // Начинаем с 1, чтобы не выдать адрес 0
        if (!ptest(i)) {
            pset(i);
            free_blocks--;
            
            uint32_t addr = i * PAGE_SIZE;

            return (void*)addr;
        }
    }
    return 0;
}


// функция выделения нескольких блоков (пригодится для DMA или стеков)
void* pmm_alloc_blocks(uint32_t count) {
    uint32_t continuous_found = 0;
    uint32_t start_bit = 0;

    for (uint32_t i = 1; i < total_blocks; i++) {
        if (!ptest(i)) {
            if (continuous_found == 0) start_bit = i;
            continuous_found++;
            if (continuous_found == count) {
                for (uint32_t j = start_bit; j < start_bit + count; j++) pset(j);
                free_blocks -= count;
                return (void*)(start_bit * PAGE_SIZE);
            }
        } else {
            continuous_found = 0;
        }
    }
    return 0;
}

//функция перевода адреса в номер бита 
void pmm_free_block(void* addr) {
    uint32_t bit = (uint32_t)addr / PAGE_SIZE;
    punset(bit);
    free_blocks++;
}

//функция привязки физ адреса к виртуальному и обнуляет его в процессе
void* kmalloc_page() {
    void* phys = pmm_alloc_block();
    if (!phys) return 0;

    uint32_t virt = next_virt_addr;
    vmm_map(virt, (uint32_t)phys, PAGE_WRITE);
    
    // БЕЗОПАСНАЯ ОЧИСТКА ТУТ
    uint32_t* ptr = (uint32_t*)virt;
    for(int i = 0; i < 1024; i++) ptr[i] = 0;

    next_virt_addr += PAGE_SIZE;
    return (void*)virt;
}

//функция демаппинга памяти
void kfree_page(void* virt_addr) {
    // Освобождаем и физику, и виртуальную связь
    vmm_unmap((uint32_t)virt_addr);
}

// Простейший аллокатор для нужд ядра
void* kmalloc(uint32_t size) {
    // Выравнивание по 4 байта
    size = (size + 3) & ~3;

    // Инициализация кучи при первом вызове
    if (heap_start == NULL) {
        heap_start = (block_header_t*)kmalloc_page(); // Берем первую страницу
        heap_start->size = PAGE_SIZE - sizeof(block_header_t);
        heap_start->is_free = 1;
        heap_start->next = NULL;
    }

    block_header_t* curr = heap_start;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Если блок слишком большой, можно его разбить (Split)
            if (curr->size > size + sizeof(block_header_t) + 4) {
                block_header_t* next_block = (block_header_t*)((uint32_t)curr + sizeof(block_header_t) + size);
                next_block->size = curr->size - size - sizeof(block_header_t);
                next_block->is_free = 1;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->is_free = 0;
            return (void*)((uint32_t)curr + sizeof(block_header_t));
        }
        
        // Если дошли до конца и не нашли места — расширяем кучу
        if (curr->next == NULL) {
            void* new_page = kmalloc_page();
            if (!new_page) return NULL; // Совсем нет памяти

            block_header_t* new_block = (block_header_t*)new_page;
            new_block->size = PAGE_SIZE - sizeof(block_header_t);
            new_block->is_free = 1;
            new_block->next = NULL;
            curr->next = new_block;
        }
        curr = curr->next;
    }
    return NULL;
}

// освобождение памяти
void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* header = (block_header_t*)((uint32_t)ptr - sizeof(block_header_t));
    header->is_free = 1;

    // Склеиваем свободные блоки, идущие подряд
    block_header_t* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            // Если текущий и следующий свободны — объединяем
            curr->size += curr->next->size + sizeof(block_header_t);
            curr->next = curr->next->next;
            // Не двигаемся дальше, проверяем еще раз этот же блок
            continue; 
        }
        curr = curr->next;
    }
}
