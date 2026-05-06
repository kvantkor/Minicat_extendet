#include "shell.h"
#include "vga.h"
#include "pmm.h"
#include "rtc.h"
#include "fat32.h"
#include "kernel.h"
#include "cpu_proc_list.h"
#include "vmm.h"

/*
 * это файл драйвера shell
 * функция сравнения строк
 * функция инициализации shell
 * функция выполнения введённой комманды 
 * функция обработки спец символа
 * функция сравнения строки с цифрой
 * функция вывода списка процессов
 * функция преобразования строки в число
*/

#define MAX_BUF 128
static char buffer[MAX_BUF];
static int ptr = 0;
extern uint32_t total_blocks;
extern uint32_t free_blocks;
extern void shell_list_procs();
extern int _atoi(char *s);
int _strncmp(const char *s1, const char *s2, int n);

// функция сравнения строк
int _strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

//инициализация оболочки shell
void shell_init() {
    ptr = 0;
    for(int i=0; i<MAX_BUF; i++) buffer[i] = 0;
    vga_puts("\nMiniCat OS Shell v1.0\n> ");
}

// функция выполнения вводимой комманды и сопоставления с функциями ядра
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
        uint8_t* load_addr = (uint8_t*)kmalloc_page();
        if (load_addr == 0) {
            vga_puts("Error: Out of memory!\n");
            return;
        }

        vga_puts("Loading "); vga_puts(filename); vga_puts("...\n");
        fat32_read_file(cluster, load_addr);

        // --- ВОТ ЗДЕСЬ ВСЕ ПЕРЕМЕННЫЕ УЖЕ ДОСТУПНЫ ---
        uint32_t kernel_pd = vmm_get_kernel_pd();
        
        vga_puts("Binary loaded at: "); vga_put_hex((uint32_t)load_addr); vga_puts("\n");
        vga_puts("Kernel PD (CR3): "); vga_put_hex(kernel_pd); vga_puts("\n");

        if (kernel_pd == 0) {
            vga_puts("CRITICAL: vmm_get_kernel_pd() returned 0!\n");
        }

        task_t* new_proc = create_task((void (*)())load_addr, kernel_pd, filename);
        
        if (new_proc) {
            vga_puts("Process started. PID: "); vga_put_int(new_proc->id);
            vga_puts(" ESP: "); vga_put_hex(new_proc->esp); vga_puts("\n");
            
            // Если хочешь увидеть эти надписи до того, как всё упадет, 
            // временно раскомментируй строку ниже:
        } else {
            vga_puts("Error: Failed to create task.\n");
            kfree_page(load_addr);
        }
    }


		
			//vga_puts("Switching to CR3: "); vga_put_hex(current_task->page_directory);
			
			//char* filename = cmd + 4;
			//uint32_t cluster = fat32_find_file(filename);
    
			//if (cluster == 0) {
			
				//vga_puts("Binary not found.\n");
			//} else {
				//// 1. Выделяем память под код программы
				//// Используем kmalloc_page, чтобы у программы была своя страница (4КБ)
				//uint8_t* load_addr = (uint8_t*)kmalloc_page();
        
				//if (load_addr == 0) {
					//vga_puts("Error: Out of memory!\n");
					//return;
				//}

			//vga_puts("Loading "); vga_puts(filename); vga_puts("...\n");
        
			//// 2. Читаем файл в выделенную память
			//fat32_read_file(cluster, load_addr);
        
			//// 3. Вместо прямого вызова entry(), создаем задачу в планировщике
			//vga_puts("Starting process...\n");
			//vga_puts("Switching to CR3: "); vga_put_hex(current_task->page_directory);
			//while(1) {}
			//task_t* new_proc = create_task((void (*)())load_addr, vmm_get_kernel_pd(), filename);
			
			//if (new_proc) {
				//vga_puts("Process '"); vga_puts(filename); 
				//vga_puts("' started with PID: ");
				//vga_put_int(new_proc->id);
				//vga_puts("\n");
			//} else {
				//vga_puts("Error: Failed to create task.\n");
				//kfree_page(load_addr); // Если не создали — возвращаем память
			//}
		//}
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

//функция обработки спец символов
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

//функция сравнения стоки с цифрой
int _strncmp(const char *s1, const char *s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    return (n == 0) ? 0 : *(unsigned char *)s1 - *(unsigned char *)s2;
}

//функция вывода списка процессов
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

//функция преобразования строки в число
int _atoi(char *s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}
