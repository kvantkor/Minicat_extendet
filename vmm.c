#include "pmm.h"
#include "vmm.h"
#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

static uint32_t* page_directory = 0;

uint32_t vmm_get_directory() {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Функция маппинга одной страницы
void vmm_map(uint32_t virtual, uint32_t physical, uint32_t flags) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        // pmm_alloc_block возвращает физический адрес
        uint32_t new_table = (uint32_t)pmm_alloc_block();
        // Записываем его в каталог с флагами
        page_directory[pd_index] = new_table | PAGE_PRESENT | PAGE_WRITE | flags;
    }

    uint32_t* table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    table[pt_index] = (physical & ~0xFFF) | PAGE_PRESENT | flags;

    asm volatile("invlpg (%0)" :: "r"(virtual) : "memory");
}

void vmm_init() {
    // Получаем физический адрес для каталога
    page_directory = (uint32_t*)pmm_alloc_block();

    // Identity map первых 4МБ
    for (uint32_t addr = 0; addr < 0x400000; addr += PAGE_SIZE) {
        vmm_map(addr, addr, PAGE_WRITE);
    }

    // Загружаем физический адрес каталога в CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

void vmm_unmap(uint32_t virtual) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    // Проверяем, существует ли вообще таблица для этого адреса
    if (!(page_directory[pd_index] & PAGE_PRESENT)) return;

    uint32_t* table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    
    // 1. Получаем физический адрес фрейма, чтобы вернуть его в PMM
    uint32_t phys = table[pt_index] & ~0xFFF;
    if (phys != 0) {
        pmm_free_block((void*)phys);
    }

    // 2. Зануляем запись (делаем её "пустой" для процессора)
    table[pt_index] = 0;

    // 3. Инвалидация кэша TLB
    asm volatile("invlpg (%0)" :: "r"(virtual) : "memory");
}
