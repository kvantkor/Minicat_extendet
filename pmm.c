#include "pmm.h"
#include "vga.h"
#include "vmm.h"

#define BLOCKS_PER_BYTE 8

// Метка конца ядра из linker.ld
extern uint32_t end;

uint32_t free_blocks;
uint32_t total_blocks;
static uint8_t* pmm_bitmap;
static uint32_t bitmap_size;
// static uint32_t next_free_addr = 0;
// static uint32_t current_page_end = 0;
static uint32_t next_virt_addr = 0x400000; // Начнем выделять выше первых 4МБ

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
	free_blocks = 0;
    total_blocks = mem_size / PAGE_SIZE;
    bitmap_size = total_blocks / 8;
    pmm_bitmap = (uint8_t*)&end;
    uint32_t bitmap_pages = (bitmap_size / PAGE_SIZE) + 1;

    // Сначала всё занято
    for (uint32_t i = 0; i < bitmap_size; i++) pmm_bitmap[i] = 0xFF;

    // Освобождаем только то, что выше ядра + битмапа
    uint32_t first_free_block = ((uint32_t)pmm_bitmap + bitmap_size) / PAGE_SIZE + 1;
    
	uint32_t last_block = total_blocks; 


    for (uint32_t i = first_free_block; i < last_block; i++) {
        punset(i);
        free_blocks++;
    }
}

void* pmm_alloc_block() {
    for (uint32_t i = 1; i < total_blocks; i++) { // Начинаем с 1, чтобы не выдать адрес 0
        if (!ptest(i)) {
            pset(i);
            free_blocks--;
            
            uint32_t addr = i * PAGE_SIZE;
            
            // ОЧИСТКА: Чтобы "плохая" пустая память стала хорошей и предсказуемой
            // Это критично для таблиц страниц!
            uint32_t* ptr = (uint32_t*)addr;
            for(int j = 0; j < 1024; j++) ptr[j] = 0;

            return (void*)addr;
        }
    }
    return 0;
}


// Добавим функцию выделения нескольких блоков (пригодится для DMA или стеков)
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

void pmm_free_block(void* addr) {
    uint32_t bit = (uint32_t)addr / PAGE_SIZE;
    punset(bit);
    free_blocks++;
}

void* kmalloc_page() {
    void* phys = pmm_alloc_block();
    if (!phys) return 0;

    uint32_t virt = next_virt_addr;
    vmm_map(virt, (uint32_t)phys, PAGE_WRITE);
    
    next_virt_addr += PAGE_SIZE;
    return (void*)virt;
}

void kfree_page(void* virt_addr) {
    // Освобождаем и физику, и виртуальную связь
    vmm_unmap((uint32_t)virt_addr);
}

// Простейший аллокатор для нужд ядра
void* kmalloc(uint32_t size) {
    // Используем статическую переменную для хранения текущей свободной позиции
    // Начинаем выделение сразу после области, занятой битмапом
    static uint32_t current_ptr = 0;

    if (current_ptr == 0) {
        // Устанавливаем начальный адрес: конец ядра + размер битмапа
        current_ptr = (uint32_t)&end + bitmap_size + 4096;
    }

    // Выравнивание адреса по 4 байта (важно для структур в x86)
    if (current_ptr % 4 != 0) {
        current_ptr += (4 - (current_ptr % 4));
    }

    void* alloc_address = (void*)current_ptr;
    current_ptr += size;

    // Простая проверка на переполнение (опционально)
    // if (current_ptr > total_blocks * PAGE_SIZE) return 0;

    return alloc_address;
}
