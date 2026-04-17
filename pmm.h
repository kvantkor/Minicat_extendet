#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

// --- Физический менеджер (работает с фреймами) ---
void pmm_init(uint32_t mem_size);
void* pmm_alloc_block();
void* pmm_alloc_blocks(uint32_t count);
void pmm_free_block(void* addr);

// --- Виртуальный менеджер / Аллокатор (работает с адресами ядра) ---
// Если решишь перенести их в vmm.h — просто вырежи отсюда
void* kmalloc(uint32_t size);
void* kmalloc_page();
void kfree_page(void* virt_addr);

#endif
