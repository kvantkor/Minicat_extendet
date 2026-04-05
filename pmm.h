#ifndef PMM_H
#define PMM_H

#include <stdint.h>

// Размер одной страницы памяти (стандарт x86)
#define PAGE_SIZE 4096

/**
 * Инициализация менеджера физической памяти.
 * @param mem_size Общий объем обнаруженной памяти в байтах.
 */
void pmm_init(uint32_t mem_size);

/**
 * Выделяет один свободный блок (4 КБ) физической памяти.
 * @return Указатель на начало блока или 0, если память кончилась.
 */
void* pmm_alloc_block();

/**
 * Освобождает ранее выделенный блок физической памяти.
 * @param addr Физический адрес блока.
 */
void pmm_free_block(void* addr);

void pmm_init(uint32_t mem_size);
void* kmalloc(uint32_t size);


#endif
