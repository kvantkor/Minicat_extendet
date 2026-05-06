#ifndef MBOOT_INFO_H
#define MBOOT_INFO_H

#include <stdint.h>

//структура multiboot для наложения
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed)) multiboot_info_t;

//структура памяти-карта
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

// Глобальные переменные-"полочки"
// общий объём памяти 
extern uint32_t RAM_TOTAL_SIZE;
//указатель на карту памяти
extern uint32_t MMAP_PTR;
//длинна карты памяти
extern uint32_t MMAP_LEN;
//строка параметров запуска
extern char*    BOOT_CMDLINE;
//адрес конца ядра
extern uint32_t KERNEL_END_ADDR;

// функция распределитель для структуры
void unpack_multiboot(uint32_t magic, uint32_t addr);

#endif
