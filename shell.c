#include "shell.h"
#include "vga.h"
#include "pmm.h"
#include "rtc.h"
#include "fat32.h"
#include "kernel.h"
#include "cpu_proc_list.h"

#define MAX_BUF 128
static char buffer[MAX_BUF];
static int ptr = 0;
extern uint32_t total_blocks;
extern uint32_t free_blocks;
extern void shell_list_procs();
extern int _atoi(char *s);
int _strncmp(const char *s1, const char *s2, int n);

// Нам понадобится strcmp, вынеси её в utils.c или оставь здесь как static
int _strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void shell_init() {
    ptr = 0;
    for(int i=0; i<MAX_BUF; i++) buffer[i] = 0;
    vga_puts("\nMiniCat OS Shell v1.0\n> ");
}

void shell_execute(char* cmd) {
    if (_strcmp(cmd, "help") == 0) {
        vga_puts("Commands: help, cls, mem, hi, time\n");
        vga_puts("Commands: cat [name], run[name], ls\n");
        vga_puts("commands: write [name] [text], list_proc, kill [pid_process]\n");
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
		
		} else if (_strncmp(cmd, "write ", 6) == 0) {
        // Формат: write filename.txt hello_world
        char* args = cmd + 6;
        char filename[13] = {0}; // FAT32 8.3 имя + \0
        
        // Разделяем имя файла и текст по первому пробелу
        int i = 0;
        while (args[i] != ' ' && args[i] != '\0' && i < 12) {
            filename[i] = args[i];
            i++;
        }
        filename[i] = '\0';

        if (args[i] == '\0') {
            vga_puts("Usage: write [filename] [text]\n");
        } else {
            char* text = args + i + 1;
            uint32_t len = 0;
            while(text[len] != '\0') len++;
            
            if (_strcmp(filename, "KERNEL.BIN") == 0) {
				vga_puts("[SEC] Access Denied: Kernel is read-only!\n");
				return;
			}


            // Вызываем функцию FAT32 (тебе нужно будет её реализовать)
            int res = fat32_write_file(filename, (uint8_t*)text, len);
            
            if (res == 0) vga_puts("Written successfully.\n");
            else vga_puts("Write error.\n");
        }
	
	}else if (_strcmp(cmd, "list_proc") == 0) {
	shell_list_procs();
	}else if (_strncmp(cmd, "kill", 4) == 0){
		int pid = _atoi(cmd + 5); // Берем всё, что после "kill "
		
		if(pid == 3){ vga_puts("Error!: cleaner its not kill!\n"); return; }
		
		task_t *target = find_task(pid);
    
		if (target != NULL) {
			if (pid < 2) { // Защита: Kernel (0) и Shell (1)
				vga_puts("Error: System protection fault!\n");
			} else {
				target->stat = TASK_STATE_KILLED; // Ставим метку
				vga_puts("Target marked for death.\n");
			}
		} else {
			vga_puts("Process not found.\n");
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

void shell_list_procs() {
    task_t* start = current_task;
    task_t* it = start;
    if (!it) return;
    
    if (it == NULL) {
        vga_puts("No tasks found.\n");
        return;
    }

    vga_puts("PID  NAME                 STATUS\n");
    do {
        // 1. PID
        vga_put_int(it->id);
        vga_puts("    ");

        // 2. Имя с примитивным выравниванием (до 20 символов)
        vga_puts(it->name);
        int len = 0;
        while(it->name[len]) len++;
        for(int i = 0; i < (20 - len); i++) vga_puts(" ");

        // 3. Статус текстом вместо непонятных чисел
        if (it->stat == 0) vga_puts("RUNNING");
        else if (it->stat == 1) vga_puts("SLEEP");
        else if (it->stat == 2) vga_puts("ZOMBIE");
        else vga_put_int(it->stat);

        vga_puts("\n");
        it = it->next;
    } while (it != start);
}

int _atoi(char *s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}
