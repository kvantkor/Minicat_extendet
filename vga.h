#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// Стандартные цвета VGA
#define VGA_COLOR_WHITE_ON_BLACK 0x0F
#define VGA_COLOR_RED_ON_BLACK   0x0C

// инициализация vga
void vga_init();
//вывод буквы
void vga_putc(char c);
//вывод строки
void vga_puts(const char* str);
//очистка экрана
void vga_clear();
//вывод числа
void vga_put_int(uint32_t num);
//вывод числа в hex формате
void vga_put_hex(uint32_t n);

#endif

