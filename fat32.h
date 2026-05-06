#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

/**
 * Структура записи в каталоге FAT32 (32 байта)
 * __attribute__((packed)) гарантирует отсутствие пропусков между полями
 */
typedef struct {
    uint8_t  name[8];          // Имя файла (8 символов, дополняется пробелами)
    uint8_t  ext[3];           // Расширение (3 символа)
    uint8_t  attr;             // Атрибуты (только чтение, скрытый, директория и т.д.)
    uint8_t  nt_res;           // Зарезервировано для NT
    uint8_t  crt_time_ten;     // Время создания (десятые доли секунды)
    uint16_t crt_time;         // Время создания
    uint16_t crt_date;         // Дата создания
    uint16_t lst_acc_date;     // Дата последнего доступа
    uint16_t cluster_high;     // Старшие 16 бит номера первого кластера
    uint16_t wrt_time;         // Время последней записи
    uint16_t wrt_date;         // Дата последней записи
    uint16_t cluster_low;      // Младшие 16 бит номера первого кластера
    uint32_t size;             // Размер файла в байтах
} __attribute__((packed)) fat32_entry_t;

// Инициализация драйвера: чтение BPB, расчет смещений таблиц FAT и данных
void fat32_init();

// Вывод списка всех файлов и папок в корневом каталоге (команда 'ls')
void fat32_list_root();

// Смещение раздела на диске (в секторах)
extern uint32_t partition_offset; 

// Поиск файла по имени. Возвращает первый кластер файла или 0, если не найден
uint32_t fat32_find_file(const char* name);

// Чтение всех данных файла в буфер (идет по цепочке кластеров в FAT)
void fat32_read_file(uint32_t cluster, uint8_t* buffer);

// Чтение значения из таблицы FAT для перехода к следующему фрагменту файла
uint32_t fat32_get_next_cluster(uint32_t current_cluster);

/**
 * Вспомогательная функция: собирает полный 32-битный номер кластера из двух частей
 */
static inline uint32_t get_cluster_from_entry(fat32_entry_t* entry) {
    return ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
}

// Обновление размера и начального кластера в записи существующего файла
void fat32_update_entry(const char* filename, uint32_t new_size, uint32_t new_cluster);

// Создание пустой записи файла в первой свободной ячейке каталога
int fat32_create_file(const char* filename);

// Удаление файла: затирание записи в каталоге (0xE5) и освобождение цепочки кластеров
int fat32_delete_file(const char *filename);

// Запись данных в файл: выделение кластеров, заполнение их данными и обновление таблицы FAT
int fat32_write_file(const char* filename, uint8_t* data, uint32_t size);

#endif
