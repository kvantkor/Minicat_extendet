#ifndef IDE_H
#define IDE_H
#include <stdint.h>

/**
 * Читает один сектор (512 байт) с диска по адресу LBA.
 * @param lba - логический адрес блока (номер сектора на диске)
 * @param buffer - указатель на память, куда будут скопированы данные
 */
void ide_read_sector(uint32_t lba, uint8_t* buffer);

/**
 * Записывает один сектор (512 байт) на диск по адресу LBA.
 * @param lba - логический адрес блока
 * @param buffer - указатель на данные, которые нужно сохранить на диск
 */
void ide_write_sector(uint32_t lba, uint8_t *buffer);

/**
 * Флаг прерывания от диска. 
 * Устанавливается в обработчике прерываний (IRQ 14/15), 
 * чтобы драйвер знал, что диск закончил операцию.
 */
extern volatile uint8_t ide_irq_fired;

#endif
