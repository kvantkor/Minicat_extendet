#include "shell.h"
#include "vga.h"
#include "pmm.h"
#include "rtc.h"

#define MAX_BUF 128
static char buffer[MAX_BUF];
static int ptr = 0;
extern uint32_t total_blocks;
extern uint32_t free_blocks;

// Нам понадобится strcmp, вынеси её в utils.c или оставь здесь как static
static int _strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void shell_init() {
    ptr = 0;
    for(int i=0; i<MAX_BUF; i++) buffer[i] = 0;
    vga_puts("\nMiniCat OS Shell v0.1\n> ");
}

void shell_execute(char* cmd) {
    if (_strcmp(cmd, "help") == 0) {
        vga_puts("Commands: help, cls, mem, hi, time\n");
    } else if (_strcmp(cmd, "cls") == 0) {
        vga_clear();
    } else if (_strcmp(cmd, "hi") == 0) {
        vga_puts("Hi! Minicat os slushaet..\n");
    } else if (_strcmp(cmd, "mem") == 0) {
		vga_puts("Total pages: ");
		vga_put_int(total_blocks);
		vga_puts("\n");
		vga_puts("Total free: ");
		vga_put_int(free_blocks);
		vga_puts("\n");
    } else if (_strcmp(cmd, "time") == 0) {
    int h, m, s;
    rtc_get_time(&h, &m, &s);
    vga_puts("Current time: ");
    vga_put_int(h); vga_putc(':');
    vga_put_int(m); vga_putc(':');
    vga_put_int(s); vga_puts("\n");
	} else if (_strcmp(cmd, "disk") == 0){
		
		uint8_t buf[512]; // Буфер на 1 сектор
		ide_read_sector(0, buf);
		
		vga_puts("Checking FAT32 signature: ");
		
		// Выводим байты с 82 по 89 как символы
		for(int i = 82; i < 90; i++) {
			vga_putc((char)buf[i]);
		}
		vga_putc('\n');
    
    } else if (cmd[0] != '\0') {
        vga_puts("Unknown: ");
        vga_puts(cmd);
        vga_puts("\n");
    }
    vga_puts("> ");
}

void shell_handle_char(char c) {
    if (c == '\n') {
        vga_putc('\n');
        buffer[ptr] = '\0';
        shell_execute(buffer);
        ptr = 0;
    } else if (c == '\b') {
        if (ptr > 0) {
            ptr--;
            vga_putc('\b');
        }
    } else if (ptr < MAX_BUF - 1) {
        buffer[ptr++] = c;
        vga_putc(c);
    }
}
