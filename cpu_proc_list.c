#include <stdint.h>
#include "kernel.h"
#include "pmm.h"
#include "vga.h"
#include "vmm.h"

/* Этот файл отвечает частично за реализацию менеджера процессов
 * здесь функция создания и удаления процесса
*/

// функция убийца выставляет метку по которй идёт отличие обычных от убитых
void shell_kill_proc(int pid) {
    task_t* it = current_task;
    
    do {
        if (it->id == ((uint32_t)pid)) {
            if (pid == 0 || pid == 1) {
                vga_puts("Access Denied: Cannot kill system process!\n");
                return;
            }
            it->stat = 2; // Просто ставим метку
            vga_puts("Task marked for termination.\n");
            return;
        }
        it = it->next;
    } while (it != current_task);
    vga_puts("PID not found.\n");
}

// создаёт процесс
task_t* create_task(void (*entry_point)(), uint32_t pdir, const char* name) {
	task_t *t = (task_t*)kmalloc(sizeof(task_t));
    t->id = next_pid++;
    t->page_directory = pdir;
    t->resources = NULL;
    t->stat = 0;

    // 1. Выделяем память под стек и сохраняем базу для будущего удаления
    uint32_t stack_mem = (uint32_t)kmalloc_page();
    if (stack_mem == 0) return NULL; // Всегда проверяй на NULL
    t->steck_base = stack_mem; 
    vmm_map(stack_mem, stack_mem, 0x3); 

    // 2. Указатель для настройки стека (начинаем с конца страницы)
    uint32_t *esp = (uint32_t*)(stack_mem + 4096 - 64);
	*(--esp) = (uint32_t)task_exit_trap;
    // 3. Копируем имя
    uint32_t i;
    for(i=0; i<19 && name[i] != '\0'; i++) {
        t->name[i] = name[i];
    }
    t->name[i] = '\0';
    
	//vga_put_hex((uint32_t)entry_point);
    // Формируем структуру registers_t в стеке (снизу вверх для процессора)
    *(--esp) = 0x202;           // eflags (IF=1, прерывания разрешены)
    *(--esp) = 0x08;            // cs (сегмент кода)
    *(--esp) = (uint32_t)entry_point; // eip (точка входа)
    
    *(--esp) = 0;               // err_code
    *(--esp) = 0;               // int_no
    
    // Регистры pushad (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    for(int i = 0; i < 8; i++) *(--esp) = 0;
    
    // 2. СЕГМЕНТЫ (GS, FS, ES, DS)
    // Эти 4 значения восстановят твои команды pop gs, pop fs...
	*(--esp) = 0x10; // Пойдет в DS
	*(--esp) = 0x10; // Пойдет в ES
	*(--esp) = 0x10; // Пойдет в FS
	*(--esp) = 0x10; // Пойдет в GS

    t->esp = (uint32_t)esp;
	
	//vga_puts("New task ESP: "); vga_put_hex(t->esp);
	//vga_puts("\n");
	
    // Вставляем в кольцевой список
    if (current_task == NULL) {
        t->next = t;
        current_task = t;
    } else {
        t->next = current_task->next;
        current_task->next = t;
    }

    return t;
}
