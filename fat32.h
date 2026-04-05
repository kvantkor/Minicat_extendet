#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

typedef struct {
    char     name[8];      // Имя файла
    char     ext[3];       // Расширение
    uint8_t  attr;         // Атрибуты (0x10 - папка, 0x20 - архив)
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t cluster_high; // Старшие 16 бит номера кластера
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;  // Младшие 16 бит номера кластера
    uint32_t size;         // Размер в байтах
} __attribute__((packed)) fat32_entry_t;

void fat32_init();
void fat32_list_root();

#endif
