#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#define NULL ((void*)0)
#define TASK_STATE_RUNNING 0
#define TASK_STATE_SLEEP   1
#define TASK_STATE_KILLED  2

typedef enum {
    RES_VGA_SYMBOL,
    RES_MEMORY
} res_type_t;

typedef struct resource {
    res_type_t type;
    uint32_t addr;
    uint32_t value;
    struct resource *next;
} resource_t;

typedef struct task {
    uint32_t id;
    uint32_t esp;
    uint32_t steck_base;
    uint32_t page_directory;
    char name[20];
    uint32_t stat;          // <--- Сделай 32-битным для выравнивания
    resource_t *resources;
    struct task *next;
    uint32_t magic;         // <--- Добавь в самый конец для теста Stack Overflow
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
// ВНИМАНИЕ: Не пиши их реализацию здесь, они уже есть в ассемблере!
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern void outw(uint16_t port, uint16_t val);
extern uint16_t inw(uint16_t port);
extern void outl(uint16_t port, uint32_t val);
extern uint32_t inl(uint16_t port);
extern void io_wait(void);

extern task_t *current_task;
extern uint32_t next_pid;
extern task_t* find_task(uint32_t pid);
// 3. Прототип обработчика (сама реализация должна быть в kernel.c или main.c)
void kernel_main();
uint32_t exception_handler(registers_t *regs);

#endif
