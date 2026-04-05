#include "vga.h"
#include "kernel.h" // Нужен для outb

static uint16_t* const VIDEO_MEM = (uint16_t*)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;

// Двигаем мигающий аппаратный курсор через порты VGA
void update_cursor() {
    uint16_t pos = cursor_y * 80 + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear() {
    for (int i = 0; i < 80 * 25; i++) {
        VIDEO_MEM[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

static void scroll() {
    if (cursor_y >= 25) {
        // Двигаем все строки на одну вверх
        for (int i = 0; i < 24 * 80; i++) {
            VIDEO_MEM[i] = VIDEO_MEM[i + 80];
        }
        // Чистим последнюю строку
        for (int i = 24 * 80; i < 25 * 80; i++) {
            VIDEO_MEM[i] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
        }
        cursor_y = 24;
    }
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VIDEO_MEM[cursor_y * 80 + cursor_x] = (VGA_COLOR_WHITE_ON_BLACK << 8) | ' ';
        }
    } else {
        VIDEO_MEM[cursor_y * 80 + cursor_x] = (VGA_COLOR_WHITE_ON_BLACK << 8) | c;
        cursor_x++;
    }

    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }

    scroll();
    update_cursor();
}

void vga_puts(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}

void vga_put_int(uint32_t num) {
    if (num == 0) {
        vga_putc('0');
        return;
    }

    char buf[11]; // Максимум 10 цифр для 32-бит + \0
    int i = 9;
    buf[10] = '\0';

    while (num > 0) {
        buf[i--] = (num % 10) + '0';
        num /= 10;
    }

    vga_puts(&buf[i + 1]);
}
