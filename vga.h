#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// Стандартные цвета VGA
#define VGA_COLOR_WHITE_ON_BLACK 0x0F
#define VGA_COLOR_RED_ON_BLACK   0x0C

void vga_init();
void vga_putc(char c);
void vga_puts(const char* str);
void vga_clear();
void vga_put_int(uint32_t num);

#endif

