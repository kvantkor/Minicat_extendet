#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

typedef struct {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_ten;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed)) fat32_entry_t;

void fat32_init();
void fat32_list_root();
extern uint32_t partition_offset; 

// Поиск файла по имени в корневом каталоге. Возвращает начальный кластер или 0.
uint32_t fat32_find_file(const char* name);

// Чтение содержимого файла по начальному кластеру в целевой буфер.
void fat32_read_file(uint32_t cluster, uint8_t* buffer);

// Вспомогательная функция для получения следующего кластера из FAT-таблицы.
uint32_t fat32_get_next_cluster(uint32_t current_cluster);

// Вспомогательная функция для склейки cluster_high и cluster_low в uint32_t.
static inline uint32_t get_cluster_from_entry(fat32_entry_t* entry) {
    return ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
}

void fat32_update_entry(const char* filename, uint32_t new_size, uint32_t new_cluster);
uint32_t fat32_find_file(const char* filename);
void fat32_read_file(uint32_t cluster, uint8_t* target_buffer);
int fat32_create_file(const char* filename);
int fat32_delete_file(const char *filename);
int fat32_write_file(const char* filename, uint8_t* data, uint32_t size);

#endif
