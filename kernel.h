#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#define NULL ((void*)0)

// состояние процесса
#define TASK_STATE_RUNNING 0
//#define TASK_STATE_SLEEP   1
#define TASK_STATE_KILLED  2

//структура ресурсов видеопамяти и озу процесса
typedef enum {
    RES_VGA_SYMBOL,
    RES_MEMORY
} res_type_t;

//общая структура ресурсов процесса
typedef struct resource {
    res_type_t type;
    uint32_t addr;
    uint32_t value;
    struct resource *next;
} resource_t;

//структура процесса
typedef struct task {
    uint32_t id;				// id процесса
    uint32_t esp;				// указатель на стек процесса
    uint32_t steck_base;		// указатель на начало стека
    uint32_t page_directory;	// указатель на страницу памяти
    char name[20];				// имя процесса
    uint32_t stat;          	// статус процесса
    resource_t *resources;		// ресурсы процесса(используемые)
    struct task *next;			// указатель на новый процесс
    uint32_t magic;         	// была гипотеза порчи структуры и этот параметр является тестовым для проверки гипотезы(отрицательно)
} __attribute__((packed)) task_t;

//, useresp, ss;
// 1. Структура состояния процессора (добавляем packed для надежности)
typedef struct {
    uint32_t gs, fs, es, ds;      // Сегменты (пушили по одному)
    uint32_t edi, esi, ebp, esp_unused, ebx, edx, ecx, eax; // pushad
    uint32_t int_no, err_code;    // Наши push byte
    uint32_t eip, cs, eflags; // Пушит процессор
} __attribute__((packed)) registers_t;

// 2. Прототипы функций, которые реализованы в ASM (kernel.asm)
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern void outw(uint16_t port, uint16_t val);
extern uint16_t inw(uint16_t port);
extern void outl(uint16_t port, uint32_t val);
extern uint32_t inl(uint16_t port);
extern void io_wait(void);

// переменные для менеджера процессов
/// указатель на процесс
extern task_t *current_task;
// указатель на следующий процесс
extern uint32_t next_pid;
//функция поиска процесса(прототип)
extern task_t* find_task(uint32_t pid);
// 3. Прототип обработчика (сама реализация в kernel.c)
//ловушка для программ 
void task_exit_trap(void);
//главная функция
void kernel_main();
//обработчик прерываний
uint32_t exception_handler(registers_t *regs);

#endif
