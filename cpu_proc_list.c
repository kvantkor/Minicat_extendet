#include <stdint.h>
#include "kernel.h"
#include "pmm.h"

task_t* create_task(void (*entry_point)(), uint32_t pdir, const char* name) {
    task_t *t = (task_t*)kmalloc(sizeof(task_t));
    t->id = next_pid++;
    t->page_directory = pdir;
	
	uint32_t i;
	for(i=0;i<19 && name[i] != '\0';i++) {
		t->name[i] = name[i];
	}
	t->name[i] = '\0';
    // Выделяем память под стек ядра (4КБ)
    uint32_t stack_mem = (uint32_t)kmalloc(4096);
    uint32_t *esp = (uint32_t*)(stack_mem + 4096);

    // Формируем структуру registers_t в стеке (снизу вверх для процессора)
    *(--esp) = 0x10;            // ss (сегмент данных)
    *(--esp) = stack_mem + 4096;// useresp (указатель стека)
    *(--esp) = 0x202;           // eflags (IF=1, прерывания разрешены)
    *(--esp) = 0x08;            // cs (сегмент кода)
    *(--esp) = (uint32_t)entry_point; // eip (точка входа)
    
    *(--esp) = 0;               // err_code
    *(--esp) = 0;               // int_no
    
    // Регистры pushad (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    for(int i = 0; i < 8; i++) *(--esp) = 0;
    
    // 2. СЕГМЕНТЫ (GS, FS, ES, DS)
    // Эти 4 значения восстановят твои команды pop gs, pop fs...
    *(--esp) = 0x10; // GS
    *(--esp) = 0x10; // FS
    *(--esp) = 0x10; // ES
    *(--esp) = 0x10; // DS

    t->esp = (uint32_t)esp;

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
