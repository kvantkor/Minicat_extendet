#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

// 1. Структура состояния процессора (добавляем packed для надежности)
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
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

// 3. Прототип обработчика (сама реализация должна быть в kernel.c или main.c)
void kernel_main();
void exception_handler(registers_t *regs);

#endif
