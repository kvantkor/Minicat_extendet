#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

void vmm_init();
void vmm_map(uint32_t virtual, uint32_t physical, uint32_t flags);
void vmm_unmap(uint32_t virtual);
uint32_t vmm_get_directory();

#endif
