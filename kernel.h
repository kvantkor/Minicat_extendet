#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#define NULL ((void*)0)

typedef struct task {
	uint32_t id;
	uint32_t esp;
	uint32_t page_directory;
	char name [20];
	struct task *next;
} task_t;

// 1. Структура состояния процессора (добавляем packed для надежности)
typedef struct {
    uint32_t gs, fs, es, ds;      // Сегменты (пушили по одному)
    uint32_t edi, esi, ebp, esp_unused, ebx, edx, ecx, eax; // pushad
    uint32_t int_no, err_code;    // Наши push byte
    uint32_t eip, cs, eflags, useresp, ss; // Пушит процессор
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
// 3. Прототип обработчика (сама реализация должна быть в kernel.c или main.c)
void kernel_main();
uint32_t exception_handler(registers_t *regs);

#endif
