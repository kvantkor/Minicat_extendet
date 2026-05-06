#include "pmm.h"
#include "vmm.h"
#include <stdint.h>
#include "vga.h"

/*
 * это файл виртуальной памяти
 * функция получения адреса
 * функция считывания регистра cr3
 * функция инициализации vmm
 * функция демаппинга 
 * функция теста vmm
 * функция маппинга одной страницы
*/
#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

static uint32_t* page_directory = 0;
uint32_t kernel_pd_phys = 0; // сохранение адреса в vmm_init

//функция для получения адреса
uint32_t vmm_get_kernel_pd() {
    return kernel_pd_phys;
}

//функция считывания регистра cr3
uint32_t vmm_get_directory() {
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Функция маппинга одной страницы
void vmm_map(uint32_t virtual, uint32_t physical, uint32_t flags) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    // 1. Проверяем наличие таблицы страниц
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        uint32_t new_table_phys = (uint32_t)pmm_alloc_block();
        
        // Очищаем новую таблицу
        uint32_t* table_ptr = (uint32_t*)new_table_phys;
        for(int i = 0; i < 1024; i++) { table_ptr[i] = 0; }

        // Записываем таблицу в каталог
        page_directory[pd_index] = new_table_phys | PAGE_PRESENT | PAGE_WRITE | flags;
    }

    // 2. Получаем адрес таблицы и мапим физическую страницу
    uint32_t* table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    table[pt_index] = (physical & ~0xFFF) | PAGE_PRESENT | flags;

    // 3. Сброс TLB
    asm volatile("invlpg (%0)" :: "r"(virtual) : "memory");
}

//инициализация vmm
void vmm_init() {
    // Получаем адрес каталога
    page_directory = (uint32_t*)pmm_alloc_block();
    kernel_pd_phys = (uint32_t)page_directory; // <--- Сохраняем физический адрес
    
    // Чистим каталог от мусора
    for(int i = 0; i < 1024; i++) page_directory[i] = 0;

    // Identity map первых 4МБ (чтобы ядро не упало сразу)
    for (uint32_t addr = 0; addr < 0x3200000; addr += PAGE_SIZE) {
        vmm_map(addr, addr, PAGE_WRITE);
    }

    // Установка CR3
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // Включение пейджинга в CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

//функция разрыва физ. адреса и виртуального и очистка
void vmm_unmap(uint32_t virtual) {
    uint32_t pd_index = virtual >> 22;
    uint32_t pt_index = (virtual >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) return;

    uint32_t* table = (uint32_t*)(page_directory[pd_index] & ~0xFFF);
    
    uint32_t phys = table[pt_index] & ~0xFFF;
    if (phys != 0) {
        pmm_free_block((void*)phys);
    }

    table[pt_index] = 0;
    asm volatile("invlpg (%0)" :: "r"(virtual) : "memory");
}

//функция-тест vmm
void test_vmm() {
    uint32_t *ptr = (uint32_t*)0x500000; // Адрес за пределами первых 4МБ
    
    vga_puts("Testing VMM: mapping 0x500000...\n");

    // 1. Пытаемся мапить физический блок на адрес 0x500000
    void* phys = pmm_alloc_block();
    vmm_map(0x500000, (uint32_t)phys, PAGE_WRITE);

    // 2. Пробуем записать данные. Если VMM врет — будет Triple Fault (перезагрузка)
    *ptr = 0xABCDE123;

    // 3. Читаем обратно
    if (*ptr == 0xABCDE123) {
        vga_puts("VMM Test: SUCCESS! Data verified.\n");
    } else {
        vga_puts("VMM Test: FAILED! Data corruption.\n");
    }

    // 4. Освобождаем (проверка unmap)
    vmm_unmap(0x500000);
}
