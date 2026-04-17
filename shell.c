#include "shell.h"
#include "vga.h"
#include "pmm.h"
#include "rtc.h"
#include "fat32.h"

#define MAX_BUF 128
static char buffer[MAX_BUF];
static int ptr = 0;
extern uint32_t total_blocks;
extern uint32_t free_blocks;
int _strncmp(const char *s1, const char *s2, int n);

// Нам понадобится strcmp, вынеси её в utils.c или оставь здесь как static
int _strcmp(const char *s1, const char *s2) {
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
        vga_puts("Commands: cat [name], run[name], ls\n");
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
	
	} else if (_strcmp(cmd, "ls") == 0) {
    fat32_list_root();
    } else if (_strncmp(cmd, "cat ", 4) == 0) {
        char* filename = cmd + 4; 
        uint32_t cluster = fat32_find_file(filename);
        if (cluster == 0) {
            vga_puts("File not found.\n");
        } else {
            uint8_t* buf = (uint8_t*)0x200000;
            for(int i=0; i < 16384;i++) {buf[i] = 0; }
            fat32_read_file(cluster, buf);
            // buf[file_size] = '\0'; // Где file_size — реальный размер файла
            vga_puts((char*)buf);
            vga_puts("\n");
        }

    } else if (_strncmp(cmd, "run ", 4) == 0) {
        char* filename = cmd + 4;
        uint32_t cluster = fat32_find_file(filename);
        if (cluster == 0) {
            vga_puts("Binary not found.\n");
        } else {
            uint8_t* load_addr = (uint8_t*)0x300000;
            vga_puts("Loading "); vga_puts(filename); vga_puts("...\n");
            
            fat32_read_file(cluster, load_addr);
            
            vga_puts("Executing...\n");
            void (*entry)() = (void (*)())load_addr;
            entry(); // Запуск
            
            vga_puts("\n[Program finished]\n");
        }
		
	} else if (_strncmp(cmd, "create ", 7) == 0) {
        char* filename = cmd + 7;
        if (fat32_create_file(filename) == 0) {
            vga_puts("File created.\n");
        } else {
            vga_puts("Error creating file.\n");
        }

    } else if (_strncmp(cmd, "delete ", 7) == 0) {
        char* filename = cmd + 7;
        
        // --- ЗАЩИТА ЯДРА ---
        if (_strcmp(filename, "KERNEL.BIN") == 0 || _strcmp(filename, "BOOT.ASM") == 0) {
            vga_puts("[SEC] Access Denied: Cannot delete system files!\n");
        } else {
            if (fat32_delete_file(filename) == 0) {
                vga_puts("File deleted.\n");
            } else {
                vga_puts("File not found or error.\n");
            }
        }
		
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

int _strncmp(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    return (n == 0) ? 0 : *(unsigned char *)s1 - *(unsigned char *)s2;
}
